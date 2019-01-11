/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_CONFIG_FIELD_TRIAL_DEFAULT_H_
#define API_CONFIG_FIELD_TRIAL_DEFAULT_H_

#include <string>
#include "absl/strings/string_view.h"
#include "api/config/webrtc_config.h"

namespace webrtc {
class FieldTrialDefaultImplementation : public WebRtcConfig {
 public:
  std::string FindFullName(absl::string_view name) const override;
};
}  // namespace webrtc

#endif  // API_CONFIG_FIELD_TRIAL_DEFAULT_H_
