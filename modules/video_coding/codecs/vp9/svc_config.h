/* Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP9_SVC_CONFIG_H_
#define MODULES_VIDEO_CODING_CODECS_VP9_SVC_CONFIG_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "api/video/encoded_image.h"
#include "api/video_codecs/spatial_layer.h"
#include "api/video_codecs/video_codec.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/svc/scalable_video_controller.h"

namespace webrtc {

// Uses scalability mode to configure spatial layers.
std::vector<SpatialLayer> GetVp9SvcConfig(VideoCodec& video_codec);

std::vector<SpatialLayer> GetSvcConfig(
    size_t input_width,
    size_t input_height,
    float max_framerate_fps,
    size_t first_active_layer,
    size_t num_spatial_layers,
    size_t num_temporal_layers,
    bool is_screen_sharing,
    absl::optional<ScalableVideoController::StreamLayersConfig> config =
        absl::nullopt);

class SimulcastToSvcConverter {
 public:
  SimulcastToSvcConverter() = default;

  SimulcastToSvcConverter(const SimulcastToSvcConverter&) = delete;
  SimulcastToSvcConverter& operator=(const SimulcastToSvcConverter&) = delete;

  ~SimulcastToSvcConverter() = default;

  void ConvertConfig(VideoCodec& codec);

  void EncodeStarted(bool force_keyframe);

  void ConvertFrame(EncodedImage& encoded_image,
                    CodecSpecificInfo& codec_specific);

 private:
  std::vector<std::unique_ptr<ScalableVideoController>> video_controllers_;
  std::vector<ScalableVideoController::LayerFrameConfig> layer_config_;
  std::vector<bool> awaiting_frame_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP9_SVC_CONFIG_H_
