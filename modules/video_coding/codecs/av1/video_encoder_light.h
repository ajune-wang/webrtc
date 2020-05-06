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

namespace webrtc {

struct EncodedFrameLight {
  // id to match Encode request.
  int64_t id = -1;
  // Data.
  rtc::scoped_refptr<EncodedImageBufferInterface> bitstream;
  // Metadata.
  bool is_keyframe = false;
  int qp = -1;
  std::vector<CodecBufferUsage> buffers_usage;
};

struct FrameConfig {
  int64_t id = -1;
  int spatial_id = 0;
  int temporal_id = 0;
  bool force_keyframe = false;
  std::vector<CodecBufferUsage> encoder_buffers;
};

struct StreamConfiguration {
  int num_spatial_layers;
  int num_temporal_layers;
};

class VideoEncoderLight {
 public:
  virtual ~VideoEncoderLight() = default;

  // Tell encoder to reset all state, in particular to encoder next frame as
  // a key frame.
  virtual void Reset() = 0;
  // Reconfigure encoder with new structure. Does't force a key frame, leaving
  // that decision up to the encoder.
  virtual void Configure(StreamConfiguration config) = 0;
  virtual bool Encode(
      const VideoFrame& picture,
      std::vector<FrameConfig> metadata,
      std::function<void(EncodedFrameLight encoded_frame)> on_encoded) = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_VIDEO_ENCODER_LIGHT_H_
