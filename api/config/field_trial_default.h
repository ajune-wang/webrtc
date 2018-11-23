/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_CONFIG_FIELD_TRIAL_DEFAULT_H_
#define API_CONFIG_FIELD_TRIAL_DEFAULT_H_

#include "api/config/field_trial.h"

#include <string>

namespace webrtc {
class FieldTrialDefaultImplementation : public FieldTrialInterface {
 public:
  std::string FindFullName(const std::string& name) const override;
  ~FieldTrialDefaultImplementation() override = default;
};
}  // namespace webrtc

#endif  // API_CONFIG_FIELD_TRIAL_DEFAULT_H_
