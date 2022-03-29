/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_FIELD_TRIALS_H_
#define API_FIELD_TRIALS_H_

#include <map>
#include <string>

#include "absl/strings/string_view.h"
#include "api/webrtc_key_value_config.h"

namespace webrtc {

class FieldTrials : public WebRtcKeyValueConfig {
 public:
  explicit FieldTrials(const std::string& s);

  std::string Lookup(absl::string_view key) const override;

 private:
  std::string field_trial_string_;
  std::map<std::string, std::string> key_value_map_;
};

}  // namespace webrtc

#endif  // API_FIELD_TRIALS_H_
