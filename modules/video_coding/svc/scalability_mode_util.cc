/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/svc/scalability_mode_util.h"

namespace webrtc {

// TODO(bugs.webrtc.org/11607): Currently limited to the VP8 usecase.
absl::optional<int> ScalabilityModeToNumTemporalLayers(
    absl::string_view scalability_mode) {
  if (scalability_mode == "L1T1") {
    return 1;
  } else if (scalability_mode == "L1T2") {
    return 2;
  } else if (scalability_mode == "L1T3") {
    return 3;
  } else {
    return absl::nullopt;
  }
}

}  // namespace webrtc
