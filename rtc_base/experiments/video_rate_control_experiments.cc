/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/video_rate_control_experiments.h"

#include <inttypes.h>
#include <stdio.h>

#include <string>

#include "api/transport/field_trial_based_config.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

const char* kCongestionWindowFieldTrialName = "WebRTC-CwndExperiment";
const int kDefaultAcceptedQueueMs = 250;

const char* kCongestionWindowPushbackFieldTrialName =
    "WebRTC-CongestionWindowPushback";
const int kDefaultMinPushbackTargetBitrateBps = 30000;

absl::optional<int> MaybeReadCwndExperimentParameter() {
  int64_t accepted_queue_ms;
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kCongestionWindowFieldTrialName);
  int parsed_values =
      sscanf(experiment_string.c_str(), "Enabled-%" PRId64, &accepted_queue_ms);
  if (parsed_values == 1) {
    RTC_CHECK_GE(accepted_queue_ms, 0)
        << "Accepted must be greater than or equal to 0.";
    return rtc::checked_cast<int>(accepted_queue_ms);
  } else if (experiment_string.find("Enabled") == 0) {
    return kDefaultAcceptedQueueMs;
  }
  return absl::nullopt;
}

absl::optional<int> MaybeReadCongestionWindowPushbackExperimentParameter() {
  uint32_t min_pushback_target_bitrate_bps;
  std::string experiment_string = webrtc::field_trial::FindFullName(
      kCongestionWindowPushbackFieldTrialName);
  int parsed_values = sscanf(experiment_string.c_str(), "Enabled-%" PRIu32,
                             &min_pushback_target_bitrate_bps);
  if (parsed_values == 1) {
    RTC_CHECK_GE(min_pushback_target_bitrate_bps, 0)
        << "Min pushback target bitrate must be greater than or equal to 0.";
    return rtc::checked_cast<int>(min_pushback_target_bitrate_bps);
  } else if (experiment_string.find("Enabled") == 0) {
    return kDefaultMinPushbackTargetBitrateBps;
  }
  return absl::nullopt;
}

}  // namespace

VideoRateControlExperiments::VideoRateControlExperiments(
    const WebRtcKeyValueConfig* const key_value_config)
    : congestion_window_("cwnd", MaybeReadCwndExperimentParameter()),
      congestion_window_pushback_(
          "cwnd_pushback",
          MaybeReadCongestionWindowPushbackExperimentParameter()) {
  ParseFieldTrial({&congestion_window_, &congestion_window_pushback_},
                  key_value_config->Lookup("WebRTC-VideoRateControl"));
}

VideoRateControlExperiments::~VideoRateControlExperiments() = default;
VideoRateControlExperiments::VideoRateControlExperiments(
    VideoRateControlExperiments&&) = default;

VideoRateControlExperiments
VideoRateControlExperiments::ParseFromFieldTrials() {
  FieldTrialBasedConfig field_trial_config;
  return VideoRateControlExperiments(&field_trial_config);
}

VideoRateControlExperiments
VideoRateControlExperiments::ParseFromKeyValueConfig(
    const WebRtcKeyValueConfig* const key_value_config) {
  FieldTrialBasedConfig field_trial_config;
  return VideoRateControlExperiments(key_value_config ? key_value_config
                                                      : &field_trial_config);
}

absl::optional<int64_t>
VideoRateControlExperiments::GetCongestionWindowParameter() const {
  return congestion_window_.GetOptional();
}
absl::optional<uint32_t>
VideoRateControlExperiments::GetCongestionWindowPushbackParameter() const {
  return congestion_window_pushback_.GetOptional();
}

}  // namespace webrtc
