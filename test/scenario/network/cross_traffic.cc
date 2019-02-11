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

RandomWalkCrossTraffic::RandomWalkCrossTraffic(RandomWalkConfig config,
                                               TrafficRoute* traffic_route)
    : config_(config),
      traffic_route_(traffic_route),
      random_(config_.random_seed) {}
RandomWalkCrossTraffic::~RandomWalkCrossTraffic() = default;

void RandomWalkCrossTraffic::Process(Timestamp at_time) {
  if (last_process_time_.IsMinusInfinity()) {
    last_process_time_ = at_time;
  }
  TimeDelta delta = at_time - last_process_time_;
  last_process_time_ = at_time;

  if (at_time - last_update_time_ >= config_.update_interval) {
    intensity_ += random_.Gaussian(config_.bias, config_.variance) *
                  (at_time - last_update_time_).seconds<double>();
    intensity_ = rtc::SafeClamp(intensity_, 0.0, 1.0);
    last_update_time_ = at_time;
  }
  pending_size_ += TrafficRate() * delta;

  if (pending_size_ >= config_.min_packet_size &&
      last_send_time_ + config_.min_packet_interval <= at_time) {
    traffic_route_->SendPacket(pending_size_.bytes());
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

PulsedPeaksCrossTraffic::PulsedPeaksCrossTraffic(PulsedPeaksConfig config,
                                                 TrafficRoute* traffic_route)
    : config_(config), traffic_route_(traffic_route) {}
PulsedPeaksCrossTraffic::~PulsedPeaksCrossTraffic() = default;

void PulsedPeaksCrossTraffic::Process(Timestamp at_time) {
  if (last_update_time_.IsMinusInfinity()) {
    intensity_ = 0;
    last_update_time_ = at_time;
  } else if (intensity_ == 0 &&
             at_time - last_update_time_ >= config_.hold_duration) {
    intensity_ = 1;
    last_update_time_ = at_time;
    // Assume that last send was done directly before send interval.
    last_send_time_ = at_time;
  } else if (intensity_ == 1 &&
             at_time - last_update_time_ >= config_.send_duration) {
    intensity_ = 0;
    last_update_time_ = at_time;
  }

  if (last_send_time_.IsMinusInfinity()) {
    // Assume that last send was done at first call.
    last_send_time_ = at_time;
  }

  DataSize pending_size = TrafficRate() * (at_time - last_send_time_);

  if (pending_size >= config_.min_packet_size &&
      last_send_time_ + config_.min_packet_interval <= at_time) {
    traffic_route_->SendPacket(pending_size.bytes());
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

}  // namespace test
}  // namespace webrtc
