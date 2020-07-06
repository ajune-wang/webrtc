/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_VIDEO_ENCODER_H_
#define MODULES_VIDEO_CODING_VIDEO_ENCODER_H_

#include "api/video_codecs/video_codec.h"

namespace webrtc {
namespace webrtc_internal {

class VideoEncoder {
 public:
  bool Configure(const VideoCodec& codec) { return codec.active; }
};

}  // namespace webrtc_internal
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_VIDEO_ENCODER_H_
