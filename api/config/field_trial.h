/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_CONFIG_FIELD_TRIAL_H_
#define API_CONFIG_FIELD_TRIAL_H_

#include <string>

namespace webrtc {
class FieldTrialInterface {
 public:
  virtual std::string FindFullName(const std::string& name) const = 0;
  inline bool IsEnabled(const char* name) {
    return FindFullName(name).find("Enabled") == 0;
  }
  inline bool IsDisabled(const char* name) {
    return FindFullName(name).find("Disabled") == 0;
  }

  virtual ~FieldTrialInterface() = default;
};
}  // namespace webrtc

#endif  // API_CONFIG_FIELD_TRIAL_H_
