/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_DEGRADATION_PREFERENCE_H_
#define API_VIDEO_VIDEO_DEGRADATION_PREFERENCE_H_

namespace webrtc {

// Based on the spec in
// https://w3c.github.io/webrtc-pc/#idl-def-rtcdegradationpreference.
// These options are enforced on a best-effort basis. For instance, all of
// these options may suffer some frame drops in order to avoid queuing.
// TODO(sprang): Look into possibility of more strictly enforcing the
// maintain-framerate option.
enum class VideoDegradationPreference {
  // Don't take any actions based on over-utilization signals.
  kDegradationDisabled,
  // On over-use, request lower frame rate, possibly causing frame drops.
  kMaintainResolution,
  // On over-use, request lower resolution, possibly causing down-scaling.
  kMaintainFramerate,
  // Try to strike a "pleasing" balance between frame rate or resolution.
  kBalanced,
};

}  // namespace webrtc


#endif  // API_VIDEO_VIDEO_DEGRADATION_PREFERENCE_H_
