/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/simplified_acknowledged_bitrate_estimator.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {

SimplifiedAcknowledgedBitrateEstimator::SimplifiedAcknowledgedBitrateEstimator(
    const SimplifiedThroughputEstimatorSettings& settings)
    : min_packets_(settings.min_packets),
      max_packets_(settings.max_packets),
      window_duration_(settings.window_duration) {
  RTC_DCHECK(settings.enabled);
}

SimplifiedAcknowledgedBitrateEstimator::
    ~SimplifiedAcknowledgedBitrateEstimator() {}

void SimplifiedAcknowledgedBitrateEstimator::IncomingPacketFeedbackVector(
    const std::vector<PacketResult>& packet_feedback_vector) {
  RTC_DCHECK(std::is_sorted(packet_feedback_vector.begin(),
                            packet_feedback_vector.end(),
                            PacketResult::ReceiveTimeOrder()));
  for (const auto& packet : packet_feedback_vector) {
    // Insert the new packet.
    window_.push_back(packet);
    // In most cases, receive timestamps should already be in order, but in the
    // rare case where feedback packets have been reordered, we do some swaps to
    // ensure that the window is sorted.
    for (size_t i = window_.size() - 1;
         i > 0 && window_[i].receive_time < window_[i - 1].receive_time; i--) {
      std::swap(window_[i], window_[i - 1]);
    }
    // Remove old packets.
    while (window_.size() > max_packets_ ||
           (window_.size() > min_packets_ &&
            packet.receive_time - window_[0].receive_time > window_duration_)) {
      window_.pop_front();
    }
  }
}

absl::optional<DataRate> SimplifiedAcknowledgedBitrateEstimator::bitrate()
    const {
  if (window_.size() < 20)
    return absl::nullopt;

  TimeDelta largest_recv_gap(TimeDelta::ms(0));
  TimeDelta second_largest_recv_gap(TimeDelta::ms(0));
  for (size_t i = 1; i < window_.size(); i++) {
    // Find receive time gaps
    TimeDelta gap = window_[i].receive_time - window_[i - 1].receive_time;
    if (gap > largest_recv_gap) {
      second_largest_recv_gap = largest_recv_gap;
      largest_recv_gap = gap;
    } else if (gap > second_largest_recv_gap) {
      second_largest_recv_gap = gap;
    }
  }

  Timestamp min_send_time = window_[0].sent_packet.send_time;
  Timestamp max_send_time = window_[0].sent_packet.send_time;
  Timestamp min_recv_time = window_[0].receive_time;
  Timestamp max_recv_time = window_[0].receive_time;
  DataSize data_size = DataSize::bytes(0);
  for (const auto& packet : window_) {
    min_send_time = std::min(min_send_time, packet.sent_packet.send_time);
    max_send_time = std::max(max_send_time, packet.sent_packet.send_time);
    min_recv_time = std::min(min_recv_time, packet.receive_time);
    max_recv_time = std::max(max_recv_time, packet.receive_time);
    data_size += packet.sent_packet.size;
    data_size += packet.sent_packet.prior_unacked_data;
  }
  const auto& first_packet = window_[0];
  const auto& last_packet = window_[window_.size() - 1];
  data_size -= (first_packet.sent_packet.size +
                first_packet.sent_packet.prior_unacked_data) /
               2;
  data_size -= (last_packet.sent_packet.size +
                last_packet.sent_packet.prior_unacked_data) /
               2;

  TimeDelta send_duration = max_send_time - min_send_time;
  TimeDelta recv_duration = (max_recv_time - min_recv_time) - largest_recv_gap +
                            second_largest_recv_gap;
  TimeDelta duration = std::max(send_duration, recv_duration);
  return data_size / std::max(duration, TimeDelta::ms(1));
}

}  // namespace webrtc
