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

#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace test {

CrossTraffic::CrossTraffic(
    Clock* clock,
    EmulatedNetworkReceiverInterface* receiver,
    uint64_t dest_endpoint_id,
    std::unique_ptr<CrossTrafficSendStrategy> send_strategy)
    : clock_(clock),
      receiver_(receiver),
      dest_endpoint_id_(dest_endpoint_id),
      send_strategy_(std::move(send_strategy)),
      last_process_time_(Timestamp::MinusInfinity()) {}
CrossTraffic::~CrossTraffic() = default;

uint64_t CrossTraffic::GetDestinationId() const {
  return dest_endpoint_id_;
}

void CrossTraffic::TriggerPacketBurst(size_t num_packets, size_t packet_size) {
  for (size_t i = 0; i < num_packets; ++i) {
    receiver_->OnPacketReceived(EmulatedIpPacket(
        rtc::SocketAddress("127.0.0.1", 90) /*from*/,
        rtc::SocketAddress(), /*to*/
        dest_endpoint_id_, rtc::CopyOnWriteBuffer(packet_size), Now()));
  }
}

void CrossTraffic::Process(Timestamp at_time) {
  if (last_process_time_.IsMinusInfinity()) {
    last_process_time_ = at_time;
  }
  TimeDelta delta = at_time - last_process_time_;
  last_process_time_ = at_time;
  std::vector<rtc::CopyOnWriteBuffer> packets =
      send_strategy_->GetPacketsToSend(at_time, delta);
  for (auto& packet : packets) {
    receiver_->OnPacketReceived(EmulatedIpPacket(
        rtc::SocketAddress() /*from*/, rtc::SocketAddress() /*to*/,
        dest_endpoint_id_, packet, at_time));
  }
}

ColumnPrinter CrossTraffic::StatsPrinter() {
  return ColumnPrinter::Lambda(
      "cross_traffic_rate",
      [this](rtc::SimpleStringBuilder& sb) {
        sb.AppendFormat("%.0lf", TrafficRate().bps() / 8.0);
      },
      32);
}

DataRate CrossTraffic::TrafficRate() const {
  return send_strategy_->TrafficRate();
}

Timestamp CrossTraffic::Now() {
  return Timestamp::us(clock_->TimeInMicroseconds());
}

RandomWalkSendStrategy::RandomWalkSendStrategy(RandomWalkConfig config)
    : config_(config), random_(config_.random_seed) {}
RandomWalkSendStrategy::~RandomWalkSendStrategy() = default;

DataRate RandomWalkSendStrategy::TrafficRate() const {
  return config_.peak_rate * intensity_;
}

std::vector<rtc::CopyOnWriteBuffer> RandomWalkSendStrategy::GetPacketsToSend(
    Timestamp at_time,
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

  std::vector<rtc::CopyOnWriteBuffer> out;
  if (pending_size_ >= config_.min_packet_size &&
      last_send_time_ + config_.min_packet_interval <= at_time) {
    out.emplace_back(pending_size_.bytes());
    pending_size_ = DataSize::Zero();
    last_send_time_ = at_time;
  }
  return out;
}

PulsedPeaksSendStrategy::PulsedPeaksSendStrategy(PulsedPeaksConfig config)
    : config_(config) {}
PulsedPeaksSendStrategy::~PulsedPeaksSendStrategy() = default;

DataRate PulsedPeaksSendStrategy::TrafficRate() const {
  return config_.peak_rate * intensity_;
}

std::vector<rtc::CopyOnWriteBuffer> PulsedPeaksSendStrategy::GetPacketsToSend(
    Timestamp at_time,
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

  std::vector<rtc::CopyOnWriteBuffer> out;
  if (pending_size_ >= config_.min_packet_size &&
      last_send_time_ + config_.min_packet_interval <= at_time) {
    out.emplace_back(pending_size_.bytes());
    pending_size_ = DataSize::Zero();
    last_send_time_ = at_time;
  }
  return out;
}

std::vector<rtc::CopyOnWriteBuffer> IdleSendStrategy::GetPacketsToSend(
    Timestamp at_time,
    TimeDelta delta) {
  return std::vector<rtc::CopyOnWriteBuffer>(0);
}

DataRate IdleSendStrategy::TrafficRate() const {
  return DataRate::kbps(0);
}

}  // namespace test
}  // namespace webrtc
