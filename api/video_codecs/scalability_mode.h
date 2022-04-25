/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_SCALABILITY_MODE_H_
#define API_VIDEO_CODECS_SCALABILITY_MODE_H_

namespace webrtc {

// Supported scability modes.
enum class ScalabilityMode {
  L1T1,
  L1T2,
  L1T3,
  L2T1,
  L2T1h,
  L2T1_KEY,
  L2T2,
  L2T2_KEY,
  L2T2_KEY_SHIFT,
  L2T3_KEY,
  L3T1,
  L3T3,
  L3T3_KEY,
  S2T1,
  S3T3,
};

}  // namespace webrtc
#endif  // API_VIDEO_CODECS_SCALABILITY_MODE_H_
