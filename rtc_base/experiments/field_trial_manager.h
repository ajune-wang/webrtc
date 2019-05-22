/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EXPERIMENTS_FIELD_TRIAL_MANAGER_H_
#define RTC_BASE_EXPERIMENTS_FIELD_TRIAL_MANAGER_H_

#include <memory>
#include <string>

#include "absl/strings/string_view.h"

namespace webrtc {

class FieldTrialManager;

// This provides access to individual trials, and abstracts application
// specific logic
class FieldTrialManager {
 public:
  FieldTrialManager();
  virtual ~FieldTrialManager();

  virtual std::string FindTrial(const absl::string_view name) = 0;

  bool IsEnabled(const absl::string_view name);
  bool IsDisabled(const absl::string_view name);
};

class DefaultFieldTrialManager : public FieldTrialManager {
 public:
  DefaultFieldTrialManager();
  ~DefaultFieldTrialManager() override;

  static std::unique_ptr<FieldTrialManager> Create();

  std::string FindTrial(const absl::string_view name) override;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_FIELD_TRIAL_MANAGER_H_
