/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/bandwidth_scaler_settings.h"

#include "api/transport/field_trial_based_config.h"
#include "rtc_base/logging.h"

namespace webrtc {

BandwidthScalerSettings::BandwidthScalerSettings(
    const WebRtcKeyValueConfig* const key_value_config)
    : bitrate_state_update_interval("bitrate_state_update_interval") {
  ParseFieldTrial(
      {&bitrate_state_update_interval},
      key_value_config->Lookup("WebRTC-Video-BandwidthScalerSettings"));
}

BandwidthScalerSettings BandwidthScalerSettings::ParseFromFieldTrials() {
  FieldTrialBasedConfig field_trial_config;
  return BandwidthScalerSettings(&field_trial_config);
}

absl::optional<uint32_t> BandwidthScalerSettings::BitrateStateUpdateInterval()
    const {
  if (bitrate_state_update_interval &&
      bitrate_state_update_interval.Value() <= 0) {
    RTC_LOG(LS_WARNING)
        << "Unsupported bitrate_state_update_interval value, ignored.";
    return absl::nullopt;
  }
  return bitrate_state_update_interval.GetOptional();
}

}  // namespace webrtc
