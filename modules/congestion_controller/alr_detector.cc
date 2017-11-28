/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/alr_detector.h"

#include <algorithm>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

const int64_t kAlrWindowMs = 100;
const int64_t kAlrMaxWindowMs = 1000;

namespace webrtc {

const char AlrDetector::kScreenshareProbingBweExperimentName[] =
    "WebRTC-ProbingScreenshareBwe";
const char AlrDetector::kStrictPacingAndProbingExperimentName[] =
    "WebRTC-StrictPacingAndProbing";
const char kDefaultProbingScreenshareBweSettings[] = "1.0,2875,80,40,-60,3";

AlrDetector::AlrDetector()
    : bandwidth_usage_percent_(kDefaultAlrBandwidthUsagePercent),
      alr_start_budget_level_percent_(kDefaultAlrStartBudgetLevelPercent),
      alr_stop_budget_level_percent_(kDefaultAlrStopBudgetLevelPercent),
      alr_budget_(0, true) {}

void AlrDetector::SetScreenshareStatus(bool screenshare) {
  auto settings = ParseAlrSettingsFromFieldTrial(screenshare);
  if (settings) {
    alr_stop_budget_level_percent_ = settings->alr_stop_budget_level_percent;
    alr_start_budget_level_percent_ = settings->alr_start_budget_level_percent;
    bandwidth_usage_percent_ = settings->alr_bandwidth_usage_percent;
  }
}
AlrDetector::~AlrDetector() {}

// TODO(srte): OnPacketSent should be merged with OnBytesSent. It's probably a
// good idea to remove the dependency on IntervalBudget at the same time.
void AlrDetector::OnPacketSent(size_t packet_size, int64_t send_time_ms) {
  data_sent_ += packet_size;
  int64_t elapsed_time_ms = send_time_ms - last_update_ms_;
  if (elapsed_time_ms >= kAlrWindowMs) {
    int64_t time_delta_ms = std::min(elapsed_time_ms, kAlrMaxWindowMs);
    int64_t data_delta_bytes = data_sent_ - last_update_data_sent_;
    OnBytesSent(data_delta_bytes, time_delta_ms);
    last_update_ms_ = send_time_ms;
    last_update_data_sent_ = data_sent_;
  }
}

void AlrDetector::OnBytesSent(size_t bytes_sent, int64_t delta_time_ms) {
  alr_budget_.UseBudget(bytes_sent);
  alr_budget_.IncreaseBudget(delta_time_ms);

  if (alr_budget_.budget_level_percent() > alr_start_budget_level_percent_ &&
      !alr_started_time_ms_) {
    alr_started_time_ms_.emplace(rtc::TimeMillis());
  } else if (alr_budget_.budget_level_percent() <
                 alr_stop_budget_level_percent_ &&
             alr_started_time_ms_) {
    alr_started_time_ms_.reset();
  }
}

void AlrDetector::SetEstimatedBitrate(int bitrate_bps) {
  RTC_DCHECK(bitrate_bps);
  const auto target_rate_kbps =
      int64_t{bitrate_bps} * bandwidth_usage_percent_ / (1000 * 100);
  alr_budget_.set_target_rate_kbps(rtc::dchecked_cast<int>(target_rate_kbps));
}

rtc::Optional<int64_t> AlrDetector::GetApplicationLimitedRegionStartTime()
    const {
  return alr_started_time_ms_;
}
rtc::Optional<AlrDetector::AlrExperimentSettings>
AlrDetector::ParseAlrSettingsFromFieldTrial(bool screenshare) {
  RTC_CHECK(
      field_trial::FindFullName(kStrictPacingAndProbingExperimentName)
          .empty() ||
      field_trial::FindFullName(kScreenshareProbingBweExperimentName).empty());
  if (screenshare) {
    return ParseAlrSettingsFromFieldTrial(kScreenshareProbingBweExperimentName);
  } else {
    return ParseAlrSettingsFromFieldTrial(
        kStrictPacingAndProbingExperimentName);
  }
}

rtc::Optional<AlrDetector::AlrExperimentSettings>
AlrDetector::ParseAlrSettingsFromFieldTrial(const char* experiment_name) {
  rtc::Optional<AlrExperimentSettings> ret;
  std::string group_name = field_trial::FindFullName(experiment_name);

  const std::string kIgnoredSuffix = "_Dogfood";
  std::string::size_type suffix_pos = group_name.rfind(kIgnoredSuffix);
  if (suffix_pos != std::string::npos &&
      suffix_pos == group_name.length() - kIgnoredSuffix.length()) {
    group_name.resize(group_name.length() - kIgnoredSuffix.length());
  }

  if (experiment_name == kScreenshareProbingBweExperimentName) {
    // This experiment is now default-on with fixed settings.
    // TODO(sprang): Remove this kill-switch and clean up experiment code.
    if (group_name != "Disabled") {
      group_name = kDefaultProbingScreenshareBweSettings;
    }
  }

  if (group_name.empty())
    return ret;

  AlrExperimentSettings settings;
  if (sscanf(group_name.c_str(), "%f,%" PRId64 ",%d,%d,%d,%d",
             &settings.pacing_factor, &settings.max_paced_queue_time,
             &settings.alr_bandwidth_usage_percent,
             &settings.alr_start_budget_level_percent,
             &settings.alr_stop_budget_level_percent,
             &settings.group_id) == 6) {
    ret.emplace(settings);
    RTC_LOG(LS_INFO) << "Using ALR experiment settings: "
                        "pacing factor: "
                     << settings.pacing_factor << ", max pacer queue length: "
                     << settings.max_paced_queue_time
                     << ", ALR start bandwidth usage percent: "
                     << settings.alr_bandwidth_usage_percent
                     << ", ALR end budget level percent: "
                     << settings.alr_start_budget_level_percent
                     << ", ALR end budget level percent: "
                     << settings.alr_stop_budget_level_percent
                     << ", ALR experiment group ID: " << settings.group_id;
  } else {
    RTC_LOG(LS_INFO) << "Failed to parse ALR experiment: " << experiment_name;
  }

  return ret;
}

}  // namespace webrtc
