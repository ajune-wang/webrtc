/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_AV1_ENCODER_MANAGER_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_ENCODER_MANAGER_H_

#include <memory>
#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "api/video_codecs/video_encoder.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller.h"
#include "modules/video_coding/codecs/av1/video_encoder_light.h"

namespace webrtc {

// Interface to control encoders for a single source.
class EncoderManager {
 public:
  explicit EncoderManager(EncodedImageCallback* encoded_image_signal);

  int32_t Encode(const VideoFrame& frame,
                 const std::vector<VideoFrameType>* frame_types);

 private:
  struct SimulcastEncoding {
    bool Enabled() const { return encoder != nullptr && structure != nullptr; }

    VideoCodecType codec_type;
    std::unique_ptr<VideoEncoderLight> encoder;
    std::unique_ptr<ScalableVideoController> structure;
  };
  //  int64_t frame_id_ = 0;
  EncodedImageCallback* const encoded_image_signal_;
  std::vector<SimulcastEncoding> encodings_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_ENCODER_MANAGER_H_
