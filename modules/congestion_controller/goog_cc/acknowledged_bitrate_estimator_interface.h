/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_INTERFACE_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_INTERFACE_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/transport/network_types.h"
#include "api/transport/webrtc_key_value_config.h"
#include "api/units/data_rate.h"
#include "rtc_base/experiments/struct_parameters_parser.h"

namespace webrtc {

struct SimplifiedThroughputEstimatorSettings {
  static constexpr char kKey[] =
      "WebRTC-Bwe-SimplifiedThroughputEstimatorSettings";

  SimplifiedThroughputEstimatorSettings() = delete;
  explicit SimplifiedThroughputEstimatorSettings(
      const WebRtcKeyValueConfig* key_value_config);

  bool enabled = false;
  TimeDelta window_duration = TimeDelta::ms(250);
  unsigned min_packets = 20;
  unsigned max_packets = 250;

  std::unique_ptr<StructParametersParser> Parser();
};

class AcknowledgedBitrateEstimatorInterface {
 public:
  static std::unique_ptr<AcknowledgedBitrateEstimatorInterface> Create(
      const WebRtcKeyValueConfig* key_value_config);
  virtual ~AcknowledgedBitrateEstimatorInterface() {}

  virtual void IncomingPacketFeedbackVector(
      const std::vector<PacketResult>& packet_feedback_vector) = 0;
  virtual absl::optional<DataRate> bitrate() const = 0;
  virtual absl::optional<DataRate> PeekRate() const = 0;
  virtual void SetAlr(bool in_alr) = 0;
  virtual void SetAlrEndedTime(Timestamp alr_ended_time) = 0;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_INTERFACE_H_
