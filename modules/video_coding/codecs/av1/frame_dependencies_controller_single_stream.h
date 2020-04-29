/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_SINGLE_STREAM_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_SINGLE_STREAM_H_

#include <stdint.h>

#include <vector>

#include "absl/types/optional.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/codecs/av1/frame_dependencies_controller.h"

namespace webrtc {

// Interface to control video stream structure of an AV1 stream.
class FrameDependenciesControllerSingleStream
    : public FrameDependenciesController {
 public:
  FrameDependenciesControllerSingleStream() = default;
  explicit FrameDependenciesControllerSingleStream(
      RenderResolution max_resolution);
  ~FrameDependenciesControllerSingleStream() override = default;

  FrameDependencyStructure Structure() const override;
  std::vector<GenericFrameInfo> NextFrameConfig(
      uint32_t rtp_timestamp) override;
  bool OnEncodeDone(uint32_t rtp_timestamp,
                    bool is_keyframe,
                    GenericFrameInfo* frame_dependencies) override;

 private:
  bool key_frame_ = true;
  int64_t frame_number_ = 0;
  const absl::optional<RenderResolution> max_resolution_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_SINGLE_STREAM_H_
