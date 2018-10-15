/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/simulated_network.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace webrtc {

SimulatedNetwork::SimulatedNetwork(SimulatedNetwork::Config config,
                                   uint64_t random_seed)
    : random_(random_seed), bursting_(false) {
  SetConfig(config);
}

SimulatedNetwork::~SimulatedNetwork() = default;

void SimulatedNetwork::SetConfig(const SimulatedNetwork::Config& config) {
  rtc::CritScope crit(&config_lock_);
  config_ = config;  // Shallow copy of the struct.
  double prob_loss = config.loss_percent / 100.0;
  if (config_.avg_burst_loss_length == -1) {
    // Uniform loss
    prob_loss_bursting_ = prob_loss;
    prob_start_bursting_ = prob_loss;
  } else {
    // Lose packets according to a gilbert-elliot model.
    int avg_burst_loss_length = config.avg_burst_loss_length;
    int min_avg_burst_loss_length = std::ceil(prob_loss / (1 - prob_loss));

    RTC_CHECK_GT(avg_burst_loss_length, min_avg_burst_loss_length)
        << "For a total packet loss of " << config.loss_percent << "%% then"
        << " avg_burst_loss_length must be " << min_avg_burst_loss_length + 1
        << " or higher.";

    prob_loss_bursting_ = (1.0 - 1.0 / avg_burst_loss_length);
    prob_start_bursting_ = prob_loss / (1 - prob_loss) / avg_burst_loss_length;
  }
}

void SimulatedNetwork::PauseTransmissionUntil(int64_t until_us) {
  rtc::CritScope crit(&config_lock_);
  pause_transmission_until_us_ = until_us;
}

bool SimulatedNetwork::EnqueuePacket(PacketInFlightInfo packet) {
  Config config;
  {
    rtc::CritScope crit(&config_lock_);
    config = config_;
  }
  rtc::CritScope crit(&process_lock_);
  if (config.queue_length_packets > 0 &&
      capacity_link_.size() >= config.queue_length_packets) {
    // Too many packet on the link, drop this one.
    return false;
  }

  if (last_bucket_visit_time_us_ < 0)
    DequeueDeliverablePackets(packet.send_time_us);
  bytes_in_queue_ += packet.size;
  capacity_link_.push({packet, last_bucket_visit_time_us_});
  return true;
}

absl::optional<int64_t> SimulatedNetwork::NextDeliveryTimeUs() const {
  rtc::CritScope crit(&process_lock_);
  if (next_delivery_time_us_ == -1) {
    return absl::nullopt;
  }
  return absl::make_optional(next_delivery_time_us_);
}

std::vector<PacketDeliveryInfo> SimulatedNetwork::DequeueDeliverablePackets(
    int64_t receive_time_us) {
  int64_t time_now_us = receive_time_us;
  std::vector<PacketDeliveryInfo> packets_to_deliver;

  if (time_now_us < next_delivery_time_us_)
    return packets_to_deliver;
  next_delivery_time_us_ =
      (next_delivery_time_us_ > 0 ? next_delivery_time_us_ : time_now_us) +
      1000;

  Config config;
  double prob_loss_bursting;
  double prob_start_bursting;
  {
    rtc::CritScope crit(&config_lock_);
    config = config_;
    prob_loss_bursting = prob_loss_bursting_;
    prob_start_bursting = prob_start_bursting_;
  }
  {
    rtc::CritScope crit(&process_lock_);

    int64_t time_passed_us = last_bucket_visit_time_us_ >= 0
                                 ? time_now_us - last_bucket_visit_time_us_
                                 : 0;
    last_bucket_visit_time_us_ = time_now_us;

    // Counting bits here instead of bytes for increased precision at very low
    // bw.
    bits_pending_drain_ += (time_passed_us * config.link_capacity_kbps / 1000);

    // Pending drain cannot exceed the amount of data in queue as we cannot save
    // unused capacity for later.
    if (bits_pending_drain_ > 8 * bytes_in_queue_)
      bits_pending_drain_ = 8 * bytes_in_queue_;

    // Check the capacity link first.
    if (!capacity_link_.empty()) {
      bool needs_sort = false;
      while (!capacity_link_.empty() &&
             (static_cast<int64_t>(capacity_link_.front().packet.size * 8) <=
                  bits_pending_drain_ ||
              config.link_capacity_kbps <= 0)) {
        // Time to get this packet.
        PacketInfo packet = std::move(capacity_link_.front());
        capacity_link_.pop();
        packet.arrival_time_us =
            packet.packet.send_time_us + time_now_us - packet.arrival_time_us;

        bits_pending_drain_ -= 8 * packet.packet.size;
        bytes_in_queue_ -= packet.packet.size;

        // Drop packets at an average rate of |config_.loss_percent| with
        // and average loss burst length of |config_.avg_burst_loss_length|.
        if ((bursting_ && random_.Rand<double>() < prob_loss_bursting) ||
            (!bursting_ && random_.Rand<double>() < prob_start_bursting)) {
          bursting_ = true;
          continue;
        } else {
          bursting_ = false;
        }

        int64_t arrival_time_jitter_us = std::max(
            random_.Gaussian(config.queue_delay_ms * 1000,
                             config.delay_standard_deviation_ms * 1000),
            0.0);

        // If reordering is not allowed then adjust arrival_time_jitter
        // to make sure all packets are sent in order.
        if (!config.allow_reordering && !delay_link_.empty() &&
            packet.arrival_time_us + arrival_time_jitter_us <
                last_arrival_time_us_) {
          arrival_time_jitter_us =
              last_arrival_time_us_ - packet.arrival_time_us;
        }
        packet.arrival_time_us += arrival_time_jitter_us;
        if (packet.arrival_time_us >= last_arrival_time_us_) {
          last_arrival_time_us_ = packet.arrival_time_us;
        } else {
          needs_sort = true;
        }
        delay_link_.emplace_back(std::move(packet));
      }

      if (needs_sort) {
        // Packet(s) arrived out of order, make sure list is sorted.
        std::sort(delay_link_.begin(), delay_link_.end(),
                  [](const PacketInfo& p1, const PacketInfo& p2) {
                    return p1.arrival_time_us < p2.arrival_time_us;
                  });
      }
    }

    // Check the extra delay queue.
    while (!delay_link_.empty() &&
           time_now_us >= delay_link_.front().arrival_time_us) {
      PacketInfo packet_info = delay_link_.front();
      packets_to_deliver.emplace_back(
          PacketDeliveryInfo(packet_info.packet, packet_info.arrival_time_us));
      delay_link_.pop_front();
    }
    // printf("%zu\n", packets_to_deliver.size());
    return packets_to_deliver;
  }
}

}  // namespace webrtc
