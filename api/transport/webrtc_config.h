/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TRANSPORT_WEBRTC_CONFIG_H_
#define API_TRANSPORT_WEBRTC_CONFIG_H_

#include <string>
#include "absl/strings/string_view.h"

namespace webrtc {

// WebRtcConfig is an interface that provides a key-value mapping for
// configuring internal details of WebRTC. There's a default implementation that
// uses field trials as the underlying mapping.
class WebRtcConfig {
 public:
  virtual ~WebRtcConfig() = default;
  // Returns the configured value string for the given key. Defaults to an empty
  // string.
  virtual std::string Lookup(absl::string_view key) const = 0;
};
}  // namespace webrtc

#endif  // API_TRANSPORT_WEBRTC_CONFIG_H_
