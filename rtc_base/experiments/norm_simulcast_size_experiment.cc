/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/norm_simulcast_size_experiment.h"

#include <string>

#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
constexpr char kFieldTrial[] = "WebRTC-NormalizeSimulcastSize";
constexpr int kMinSetting = 0;
constexpr int kMaxSetting = 5;
}  // namespace

bool NormSimulcastSizeExperiment::Enabled() {
  return webrtc::field_trial::IsEnabled(kFieldTrial);
}

absl::optional<int> NormSimulcastSizeExperiment::GetBase2Exponent() {
  if (!Enabled())
    return absl::nullopt;

  const std::string group = webrtc::field_trial::FindFullName(kFieldTrial);
  if (group.empty())
    return absl::nullopt;

  int exp;
  if (sscanf(group.c_str(), "Enabled-%d", &exp) != 1) {
    RTC_LOG(LS_WARNING) << "Invalid number of parameters provided.";
    return absl::nullopt;
  }

  if (exp < kMinSetting || exp > kMaxSetting) {
    RTC_LOG(LS_WARNING) << "Unsupported exp value provided, value ignored.";
    return absl::nullopt;
  }

  return absl::optional<int>(exp);
}

}  // namespace webrtc
