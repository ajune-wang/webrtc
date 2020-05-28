/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_AV1_VIDEO_ENCODER_LIGHT_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_VIDEO_ENCODER_LIGHT_H_

#include <memory>
#include <vector>

#include "absl/base/attributes.h"
#include "api/video/video_frame.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller.h"

namespace webrtc {

struct EncodedFrameLight {
  // Data.
  rtc::scoped_refptr<EncodedImageBufferInterface> bitstream;
  // Metadata.
  int qp = -1;
  ScalableVideoController::LayerFrameConfig config;
};

class VideoEncoderLight {
 public:
  virtual ~VideoEncoderLight() = default;

  // Reconfigure encoder with new structure. Does't force a key frame, leaving
  // that decision up to the encoder.
  virtual void Configure(
      ScalableVideoController::StreamLayersConfig config) = 0;
  virtual bool Encode(
      const VideoFrame& picture,
      std::vector<ScalableVideoController::LayerFrameConfig> metadata,
      std::function<void(EncodedFrameLight encoded_frame)> on_encoded) = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_VIDEO_ENCODER_LIGHT_H_
