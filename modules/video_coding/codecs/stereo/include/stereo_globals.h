/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_STEREO_INCLUDE_STEREO_GLOBALS_H_
#define MODULES_VIDEO_CODING_CODECS_STEREO_INCLUDE_STEREO_GLOBALS_H_

namespace webrtc {

struct StereoIndices {
  uint8_t frame_index;
  uint8_t frame_count;
  uint16_t picture_index;

  uint32_t yuv_size;
  uint32_t yuv_length;
  uint8_t yuv_type;

  uint32_t alpha_size;
  uint32_t alpha_length;
  uint8_t alpha_type;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_STEREO_INCLUDE_STEREO_GLOBALS_H_
