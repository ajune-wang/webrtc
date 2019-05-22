/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/field_trial_manager.h"
#include "absl/memory/memory.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

FieldTrialManager::FieldTrialManager() {}

FieldTrialManager::~FieldTrialManager() {}

bool FieldTrialManager::IsEnabled(const absl::string_view name) {
  return FindTrial(name).find("Enabled") == 0;
}

bool FieldTrialManager::IsDisabled(const absl::string_view name) {
  return FindTrial(name).find("Disabled") == 0;
}

DefaultFieldTrialManager::DefaultFieldTrialManager() : FieldTrialManager() {}

DefaultFieldTrialManager::~DefaultFieldTrialManager() {}

std::unique_ptr<FieldTrialManager> DefaultFieldTrialManager::Create() {
  return absl::make_unique<DefaultFieldTrialManager>();
}

std::string DefaultFieldTrialManager::FindTrial(const absl::string_view name) {
  std::string trial_name = std::string("WebRTC-") + std::string(name);
  return field_trial::FindFullName(trial_name);
}

}  // namespace webrtc
