/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/jitter_upper_bound_experiment.h"

#include <algorithm>
#include <string>

#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

const char JitterUpperBoundExperiment::kJitterUpperBoundExperimentName[] =
    "WebRTC-JitterUpperBound";

absl::optional<int> JitterUpperBoundExperiment::GetUpperBoundMs() {
  if (!field_trial::IsEnabled(kJitterUpperBoundExperimentName)) {
    return absl::nullopt;
  }
  const std::string group =
      webrtc::field_trial::FindFullName(kJitterUpperBoundExperimentName);

  int upper_bound_ms;
  if (sscanf(group.c_str(), "Enabled-%u", &upper_bound_ms) != 1) {
    RTC_LOG(LS_WARNING) << "Invalid number of parameters provided.";
    return absl::nullopt;
  }

  if (upper_bound_ms < 0) {
    RTC_LOG(LS_WARNING) << "Invalid jitter upper bound, must be >= 0: "
                        << upper_bound_ms;
    return absl::nullopt;
  }

  return upper_bound_ms;
}

}  // namespace webrtc
