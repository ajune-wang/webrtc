/* Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_H265_H265_SVC_CONFIG_H_
#define MODULES_VIDEO_CODING_CODECS_H265_H265_SVC_CONFIG_H_

#include "api/video_codecs/video_codec.h"

namespace webrtc {

// H.265 SVC configuration do not support spatial layers.
void SetH265SvcConfig(VideoCodec& video_codec, int num_temporal_layers);

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_H265_H265_SVC_CONFIG_H_
