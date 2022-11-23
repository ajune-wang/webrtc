/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_ROTATION_H_
#define API_VIDEO_VIDEO_ROTATION_H_

#include <string>

namespace webrtc {

// enum for clockwise rotation.
enum VideoRotation {
  kVideoRotation_0 = 0,
  kVideoRotation_90 = 90,
  kVideoRotation_180 = 180,
  kVideoRotation_270 = 270
};

inline const char* VideoRotationToString(VideoRotation video_rotation) {
  switch (video_rotation) {
    case VideoRotation::kVideoRotation_0:
      return "0";
    case VideoRotation::kVideoRotation_90:
      return "90";
    case VideoRotation::kVideoRotation_180:
      return "180";
    case VideoRotation::kVideoRotation_270:
      return "270";
    default:
      return "UNKNOWN";
  }
}

inline bool VideoRotationFromString(std::string str, VideoRotation* ret) {
  if (str == "0") {
    *ret = VideoRotation::kVideoRotation_0;
    return true;
  }
  if (str == "90") {
    *ret = VideoRotation::kVideoRotation_90;
    return true;
  }
  if (str == "180") {
    *ret = VideoRotation::kVideoRotation_180;
    return true;
  }
  if (str == "270") {
    *ret = VideoRotation::kVideoRotation_270;
    return true;
  }
  return false;
}

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_ROTATION_H_
