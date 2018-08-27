/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/rtt_mult_experiment.h"

#include <string>

#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

const char kRttMultExperiment[] = "WebRTC-RttMult";
}  // namespace

bool RttMultExperiment::RttMultEnabled() {
  std::string trial_string =
      webrtc::field_trial::FindFullName(kRttMultExperiment);
  return trial_string.find("Enabled,RttMult") == 0;
}

// absl::optional<xxx>
float RttMultExperiment::GetRttMult() {
  const std::string group =
      webrtc::field_trial::FindFullName(kRttMultExperiment);
  if (group.empty())
    return 0.0;  // Is there a better value to return for this case?

  float rtt_mult_setting;
  if (sscanf(group.c_str(), "Enabled-%f", &rtt_mult_setting) != 1) {
    RTC_LOG(LS_WARNING) << "Invalid number of parameters provided.";
    return 0.0;  // Is there a better value to return for this case?
  }
  return rtt_mult_setting;
}

}  // namespace webrtc
