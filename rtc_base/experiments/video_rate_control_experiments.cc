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

#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

const char* kCongestionWindowFieldTrialName = "WebRTC-CwndExperiment";
const int64_t kDefaultAcceptedQueueMs = 250;

const char* kCongestionWindowPushbackFieldTrialName =
    "WebRTC-CongestionWindowPushback";
const uint32_t kDefaultMinPushbackTargetBitrateBps = 30000;

bool ReadCwndExperimentParameter(int64_t* accepted_queue_ms) {
  RTC_DCHECK(accepted_queue_ms);
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kCongestionWindowFieldTrialName);
  int parsed_values = 0;
  sscanf(experiment_string.c_str(), "Enabled-%" PRId64, accepted_queue_ms);
  if (parsed_values == 1) {
    RTC_CHECK_GE(*accepted_queue_ms, 0)
        << "Accepted must be greater than or equal to 0.";
    return true;
  }
  return false;
}

bool ReadCongestionWindowPushbackExperimentParameter(
    uint32_t* min_pushback_target_bitrate_bps) {
  RTC_DCHECK(min_pushback_target_bitrate_bps);
  std::string experiment_string = webrtc::field_trial::FindFullName(
      kCongestionWindowPushbackFieldTrialName);
  int parsed_values = sscanf(experiment_string.c_str(), "Enabled-%" PRIu32,
                             min_pushback_target_bitrate_bps);
  if (parsed_values == 1) {
    RTC_CHECK_GE(*min_pushback_target_bitrate_bps, 0)
        << "Min pushback target bitrate must be greater than or equal to 0.";
    return true;
  }
  return false;
}

}  // namespace

VideoRateControlExperiments::VideoRateControlExperiments() {
  FieldTrialOptional<int> congestion_window("cwnd", kDefaultAcceptedQueueMs);
  FieldTrialOptional<int> congestion_window_pushback(
      "cwnd_pushback", kDefaultMinPushbackTargetBitrateBps);
  ParseFieldTrial({&congestion_window, &congestion_window_pushback},
                  field_trial::FindFullName("WebRTC-VideoRateControl"));

  congestion_window_ = congestion_window.GetOptional();
  if (!congestion_window_) {
    std::string experiment_string =
        webrtc::field_trial::FindFullName(kCongestionWindowFieldTrialName);
    // The experiment is enabled iff the field trial string begins with
    // "Enabled".
    if (experiment_string.find("Enabled") == 0) {
      int64_t accepted_queue_ms;
      if (!ReadCwndExperimentParameter(&accepted_queue_ms)) {
        accepted_queue_ms = kDefaultAcceptedQueueMs;
      }
      congestion_window_ = accepted_queue_ms;
    }
  }

  congestion_window_pushback_ = congestion_window_pushback.GetOptional();
  if (!congestion_window_pushback_) {
    std::string experiment_string = webrtc::field_trial::FindFullName(
        kCongestionWindowPushbackFieldTrialName);
    // The experiment is enabled iff the field trial string begins with
    // "Enabled".
    if (experiment_string.find("Enabled") == 0) {
      uint32_t min_pushback_target_bitrate_bps;
      if (!ReadCongestionWindowPushbackExperimentParameter(
              &min_pushback_target_bitrate_bps)) {
        min_pushback_target_bitrate_bps = kDefaultMinPushbackTargetBitrateBps;
      }
      congestion_window_ = min_pushback_target_bitrate_bps;
    }
  }
}

VideoRateControlExperiments::~VideoRateControlExperiments() = default;
VideoRateControlExperiments::VideoRateControlExperiments(
    VideoRateControlExperiments&&) = default;

VideoRateControlExperiments VideoRateControlExperiments::ParseFromFieldTrial() {
  return VideoRateControlExperiments();
}

absl::optional<int64_t>
VideoRateControlExperiments::GetCongestionWindowParameter() const {
  return congestion_window_;
}
absl::optional<uint32_t>
VideoRateControlExperiments::GetCongestionWindowPushbackParameter() const {
  return congestion_window_pushback_;
}

}  // namespace webrtc
