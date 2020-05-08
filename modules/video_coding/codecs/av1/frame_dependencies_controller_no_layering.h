/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_NO_LAYERING_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_NO_LAYERING_H_

#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/codecs/av1/frame_dependencies_controller.h"

namespace webrtc {

class FrameDependenciesControllerNoLayering
    : public FrameDependenciesController {
 public:
  ~FrameDependenciesControllerNoLayering() override;

  FrameDependencyStructure DependencyStructure() const override;
  std::vector<GenericFrameInfo> NextFrameConfig(bool restart) override;

 private:
  bool start_ = true;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_NO_LAYERING_H_
