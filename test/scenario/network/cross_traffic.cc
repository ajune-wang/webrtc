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

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace test {
namespace {

class NullReceiver : public EmulatedNetworkReceiverInterface {
 public:
  void OnPacketReceived(EmulatedIpPacket packet) override{};
};

class ActionReceiver : public EmulatedNetworkReceiverInterface {
 public:
  ActionReceiver(std::function<void()> action, EndpointNode* endpoint)
      : action_(action), endpoint_(endpoint) {}
  ~ActionReceiver() override = default;

  void OnPacketReceived(EmulatedIpPacket packet) override {
    RTC_DCHECK(port_);
    action_();
    endpoint_->UnbindReceiver(port_.value());
  };

  // We can't set port in constructor, because port will be provided by
  // endpoint, when this receiver will be binded to that endpoint.
  void SetPort(uint16_t port) { port_ = port; }

 private:
  std::function<void()> action_;
  // Endpoint and port will be used to free port in the endpoint after action
  // will be done.
  EndpointNode* endpoint_;
  absl::optional<uint16_t> port_ = absl::nullopt;
};

}  // namespace

TrafficRoute::TrafficRoute(Clock* clock,
                           EmulatedNetworkReceiverInterface* receiver,
                           EndpointNode* endpoint)
    : clock_(clock), receiver_(receiver), endpoint_(endpoint) {
  null_receiver_ = absl::make_unique<NullReceiver>();
  absl::optional<uint16_t> port =
      endpoint_->BindReceiver(0, null_receiver_.get());
  RTC_DCHECK(port);
  null_receiver_port_ = port.value();
}
TrafficRoute::~TrafficRoute() = default;

void TrafficRoute::TriggerPacketBurst(size_t num_packets, size_t packet_size) {
  for (size_t i = 0; i < num_packets; ++i) {
    SendPacket(rtc::CopyOnWriteBuffer(packet_size));
  }
}

void TrafficRoute::NetworkDelayedAction(size_t packet_size,
                                        std::function<void()> action) {
  auto action_receiver = absl::make_unique<ActionReceiver>(action, endpoint_);
  absl::optional<uint16_t> port =
      endpoint_->BindReceiver(0, action_receiver.get());
  RTC_DCHECK(port);
  action_receiver->SetPort(port.value());
  actions_.push_back(std::move(action_receiver));
  SendPacket(rtc::CopyOnWriteBuffer(packet_size), port.value());
}

void TrafficRoute::SendPacket(rtc::CopyOnWriteBuffer data) {
  SendPacket(data, null_receiver_port_);
}

void TrafficRoute::SendPacket(rtc::CopyOnWriteBuffer data, uint16_t dest_port) {
  receiver_->OnPacketReceived(EmulatedIpPacket(
      /*from=*/rtc::SocketAddress(),
      rtc::SocketAddress(endpoint_->GetPeerLocalAddress(), dest_port),
      endpoint_->GetId(), data, Timestamp::us(clock_->TimeInMicroseconds())));
}

RandomWalkCrossTraffic::RandomWalkCrossTraffic(RandomWalkConfig config,
                                               TrafficRoute* cross_traffic)
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
    cross_traffic_->SendPacket(rtc::CopyOnWriteBuffer(pending_size_.bytes()));
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
                                                 TrafficRoute* cross_traffic)
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
    cross_traffic_->SendPacket(rtc::CopyOnWriteBuffer(pending_size_.bytes()));
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
