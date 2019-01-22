/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/network/cross_traffic.h"

#include <utility>

#include "absl/types/optional.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace test {

CrossTraffic::CrossTraffic(Clock* clock,
                           EmulatedNetworkReceiverInterface* receiver,
                           uint64_t dest_endpoint_id)
    : clock_(clock), receiver_(receiver), dest_endpoint_id_(dest_endpoint_id) {}
CrossTraffic::~CrossTraffic() = default;

uint64_t CrossTraffic::GetDestinationId() const {
  return dest_endpoint_id_;
}

void CrossTraffic::TriggerPacketBurst(size_t num_packets, size_t packet_size) {
  for (size_t i = 0; i < num_packets; ++i) {
    receiver_->OnPacketReceived(EmulatedIpPacket(
        // Use some dummy source address
        /*from=*/rtc::SocketAddress("127.0.0.1", 12345),
        /*to=*/rtc::SocketAddress(), dest_endpoint_id_,
        rtc::CopyOnWriteBuffer(packet_size),
        Timestamp::us(clock_->TimeInMicroseconds())));
  }
}

RandomWalkCrossTraffic::RandomWalkCrossTraffic(RandomWalkConfig config,
                                               CrossTraffic* cross_traffic)
    : config_(config),
      cross_traffic_(cross_traffic),
      random_(config_.random_seed) {}
RandomWalkCrossTraffic::~RandomWalkCrossTraffic() = default;

void RandomWalkCrossTraffic::Process(Timestamp at_time) {
  if (last_process_time_.IsMinusInfinity()) {
    last_process_time_ = at_time;
  }
  TimeDelta delta = at_time - last_process_time_;
  last_process_time_ = at_time;

  UpdatePendingSize(at_time, delta);

  if (pending_size_ >= config_.min_packet_size &&
      last_send_time_ + config_.min_packet_interval <= at_time) {
    cross_traffic_->receiver_->OnPacketReceived(EmulatedIpPacket(
        rtc::SocketAddress() /*from*/, rtc::SocketAddress() /*to*/,
        cross_traffic_->dest_endpoint_id_,
        rtc::CopyOnWriteBuffer(pending_size_.bytes()), at_time));
    pending_size_ = DataSize::Zero();
    last_send_time_ = at_time;
  }
}

DataRate RandomWalkCrossTraffic::TrafficRate() const {
  return config_.peak_rate * intensity_;
}

ColumnPrinter RandomWalkCrossTraffic::StatsPrinter() {
  return ColumnPrinter::Lambda(
      "random_walk_cross_traffic_rate",
      [this](rtc::SimpleStringBuilder& sb) {
        sb.AppendFormat("%.0lf", TrafficRate().bps() / 8.0);
      },
      32);
}

void RandomWalkCrossTraffic::UpdatePendingSize(Timestamp at_time,
                                               TimeDelta delta) {
  time_since_update_ += delta;
  if (time_since_update_ >= config_.update_interval) {
    intensity_ += random_.Gaussian(config_.bias, config_.variance) *
                  time_since_update_.seconds<double>();
    RTC_LOG(INFO) << "Intermediate intensity value: " << intensity_;
    intensity_ = rtc::SafeClamp(intensity_, 0.0, 1.0);
    time_since_update_ = TimeDelta::Zero();
    RTC_LOG(INFO) << "Intensity updated to " << intensity_;
  }
  pending_size_ += TrafficRate() * delta;
}

PulsedPeaksCrossTraffic::PulsedPeaksCrossTraffic(PulsedPeaksConfig config,
                                                 CrossTraffic* cross_traffic)
    : config_(config), cross_traffic_(cross_traffic) {}
PulsedPeaksCrossTraffic::~PulsedPeaksCrossTraffic() = default;

void PulsedPeaksCrossTraffic::Process(Timestamp at_time) {
  if (last_process_time_.IsMinusInfinity()) {
    last_process_time_ = at_time;
  }
  TimeDelta delta = at_time - last_process_time_;
  last_process_time_ = at_time;

  UpdatePendingSize(at_time, delta);

  if (pending_size_ >= config_.min_packet_size &&
      last_send_time_ + config_.min_packet_interval <= at_time) {
    cross_traffic_->receiver_->OnPacketReceived(EmulatedIpPacket(
        rtc::SocketAddress() /*from*/, rtc::SocketAddress() /*to*/,
        cross_traffic_->dest_endpoint_id_,
        rtc::CopyOnWriteBuffer(pending_size_.bytes()), at_time));
    pending_size_ = DataSize::Zero();
    last_send_time_ = at_time;
  }
}

DataRate PulsedPeaksCrossTraffic::TrafficRate() const {
  return config_.peak_rate * intensity_;
}

ColumnPrinter PulsedPeaksCrossTraffic::StatsPrinter() {
  return ColumnPrinter::Lambda(
      "pulsed_peaks_cross_traffic_rate",
      [this](rtc::SimpleStringBuilder& sb) {
        sb.AppendFormat("%.0lf", TrafficRate().bps() / 8.0);
      },
      32);
}

void PulsedPeaksCrossTraffic::UpdatePendingSize(Timestamp at_time,
                                                TimeDelta delta) {
  time_since_update_ += delta;
  if (intensity_ == 0 && time_since_update_ >= config_.hold_duration) {
    intensity_ = 1;
    time_since_update_ = TimeDelta::Zero();
  } else if (intensity_ == 1 && time_since_update_ >= config_.send_duration) {
    intensity_ = 0;
    time_since_update_ = TimeDelta::Zero();
  }
  pending_size_ += TrafficRate() * delta;
}

}  // namespace test
}  // namespace webrtc
