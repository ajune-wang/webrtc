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

#include <string>
#include <unordered_map>

#include "absl/strings/string_view.h"
#include "api/webrtc_key_value_config.h"

namespace webrtc {

// The FieldTrials class is used to inject field trials into webrtc.
//
// Field trials allow webrtc clients (such as Chrome) to turn on feature code
// in binaries out in the field and gather information with that.
//
// They are designed to be easy to use with chrome field trials and to speed up
// developers by reducing the need to wire APIs to control whether a feature is
// on/off.
//
// The field trials are (optionally) injected when creating PeerConnection,
// and they are checked inside WebRtc using a FieldTrialsView.
//
class FieldTrials : public WebRtcKeyValueConfig {
 public:
  explicit FieldTrials(const std::string& s);

  std::string Lookup(absl::string_view key) const override;

 private:
  const std::string field_trial_string_;
  const std::unordered_map<std::string, std::string> key_value_map_;
};

}  // namespace webrtc

#endif  // API_FIELD_TRIALS_H_
