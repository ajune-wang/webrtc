/*
 *  Copyright 2022 The WebRTC Project Authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TOOLS_WEBRTC_EXPERIMENTS_FIELD_TRIALS_REGISTRY_H_
#define TOOLS_WEBRTC_EXPERIMENTS_FIELD_TRIALS_REGISTRY_H_

#include <iterator>

#include "absl/strings/string_view.h"
#include "tools_webrtc/experiments/registered_field_trials.h"

namespace webrtc {

constexpr bool IsFieldTrialRegistered(absl::string_view key) {
#if RTC_STRICT_FIELD_TRIALS
  for (size_t i = 0; i < std::size(kRegisteredFieldTrials); ++i) {
    if (key == kRegisteredFieldTrials[i]) {
      return true;
    }
  }
  return false;
#else
  // Always indicate a field trial to be registered in non-strict mode.
  return true;
#endif
}

}  // namespace webrtc

#endif  // TOOLS_WEBRTC_EXPERIMENTS_FIELD_TRIALS_REGISTRY_H_
