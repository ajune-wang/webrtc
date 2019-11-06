/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator_interface.h"

#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/goog_cc/simplified_acknowledged_bitrate_estimator.h"

namespace webrtc {

constexpr char SimplifiedThroughputEstimatorSettings::kKey[];

SimplifiedThroughputEstimatorSettings::SimplifiedThroughputEstimatorSettings(
    const WebRtcKeyValueConfig* key_value_config) {
  Parser()->Parse(
      key_value_config->Lookup(SimplifiedThroughputEstimatorSettings::kKey));
}

std::unique_ptr<StructParametersParser>
SimplifiedThroughputEstimatorSettings::Parser() {
  return StructParametersParser::Create("enabled", &enabled,          //
                                        "min_packets", &min_packets,  //
                                        "max_packets", &max_packets,  //
                                        "window_duration", &window_duration);
}

std::unique_ptr<AcknowledgedBitrateEstimatorInterface>
AcknowledgedBitrateEstimatorInterface::Create(
    const WebRtcKeyValueConfig* key_value_config) {
  SimplifiedThroughputEstimatorSettings simplified_estimator_settings(
      key_value_config);
  if (simplified_estimator_settings.enabled) {
    return std::make_unique<SimplifiedAcknowledgedBitrateEstimator>(
        simplified_estimator_settings);
  }
  return std::make_unique<AcknowledgedBitrateEstimator>(key_value_config);
}

}  // namespace webrtc
