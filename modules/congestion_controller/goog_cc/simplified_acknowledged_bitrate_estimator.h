/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_SIMPLIFIED_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_SIMPLIFIED_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_

#include <deque>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/transport/network_types.h"
#include "api/transport/webrtc_key_value_config.h"
#include "api/units/data_rate.h"
#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator_interface.h"

namespace webrtc {

class SimplifiedAcknowledgedBitrateEstimator
    : public AcknowledgedBitrateEstimatorInterface {
 public:
  explicit SimplifiedAcknowledgedBitrateEstimator(
      const SimplifiedThroughputEstimatorSettings& settings);
  ~SimplifiedAcknowledgedBitrateEstimator() override;

  void IncomingPacketFeedbackVector(
      const std::vector<PacketResult>& packet_feedback_vector) override;

  absl::optional<DataRate> bitrate() const override;

  absl::optional<DataRate> PeekRate() const override { return bitrate(); }
  void SetAlr(bool /*in_alr*/) override {}
  void SetAlrEndedTime(Timestamp /*alr_ended_time*/) override {}

 private:
  const size_t min_packets_ = 20;
  const size_t max_packets_ = 250;
  const TimeDelta window_duration_ = TimeDelta::ms(250);
  std::deque<PacketResult> window_;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_SIMPLIFIED_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_
