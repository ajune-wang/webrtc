/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_FULL_SVC_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_FULL_SVC_H_

#include <stdint.h>

#include <functional>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/codecs/av1/frame_dependencies_controller.h"

namespace webrtc {

// Interface to control video stream structure of an AV1 stream.
class FrameDependenciesControllerFullSvc : public FrameDependenciesController {
 public:
  explicit FrameDependenciesControllerFullSvc(
      absl::optional<RenderResolution> max_resolution = absl::nullopt,
      int num_spatial_layers = 3,
      int num_temporal_layers = 3)
      : max_resolution_(max_resolution),
        max_spatial_layers_(num_spatial_layers),
        max_temporal_layers_(num_temporal_layers) {}
  ~FrameDependenciesControllerFullSvc() override = default;

  FrameDependencyStructure DependencyStructure() const override;
  std::vector<GenericFrameInfo> NextFrameConfig(bool reset) override;

 private:
  const absl::optional<RenderResolution> max_resolution_;
  const int max_spatial_layers_;
  const int max_temporal_layers_;
  // 0 is key frame, then pattern {1,2,3,4} repeats.
  int temporal_unit_template_idx_ = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_FULL_SVC_H_
