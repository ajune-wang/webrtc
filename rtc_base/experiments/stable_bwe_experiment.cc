/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/stable_bwe_experiment.h"

#include "api/transport/field_trial_based_config.h"
#include "rtc_base/experiments/rate_control_settings.h"

namespace webrtc {
namespace {
constexpr char kFieldTrialName[] = "WebRTC-StableBwe";
}  // namespace

StableBweExperiment::StableBweExperiment(
    const WebRtcKeyValueConfig* const key_value_config,
    absl::optional<double> default_video_hysteresis,
    absl::optional<double> default_screenshare_hysteresis)
    : enabled_("enabled", false),
      video_hysteresis_factor_("video_hysteresis_factor",
                               default_video_hysteresis),
      screenshare_hysteresis_factor_("screenshare_hysteresis_factor",
                                     default_screenshare_hysteresis) {
  ParseFieldTrial(
      {&enabled_, &video_hysteresis_factor_, &screenshare_hysteresis_factor_},
      key_value_config->Lookup(kFieldTrialName));
}

StableBweExperiment::StableBweExperiment(StableBweExperiment&&) = default;

StableBweExperiment StableBweExperiment::ParseFromFieldTrials() {
  FieldTrialBasedConfig config;
  return ParseFromKeyValueConfig(&config);
}

StableBweExperiment StableBweExperiment::ParseFromKeyValueConfig(
    const WebRtcKeyValueConfig* const key_value_config) {
  if (key_value_config->Lookup("WebRTC-VideoRateControl") != "") {
    RateControlSettings rate_control =
        RateControlSettings::ParseFromKeyValueConfig(key_value_config);
    return StableBweExperiment(key_value_config,
                               rate_control.GetSimulcastHysteresisFactor(
                                   VideoCodecMode::kRealtimeVideo),
                               rate_control.GetSimulcastHysteresisFactor(
                                   VideoCodecMode::kScreensharing));
  }
  return StableBweExperiment(key_value_config, absl::nullopt, absl::nullopt);
}

bool StableBweExperiment::IsEnabled() const {
  return enabled_.Get();
}

absl::optional<double> StableBweExperiment::GetVideoHysteresisFactor() const {
  return video_hysteresis_factor_.GetOptional();
}

absl::optional<double> StableBweExperiment::GetScreenshareHysteresisFactor()
    const {
  return screenshare_hysteresis_factor_.GetOptional();
}

}  // namespace webrtc
