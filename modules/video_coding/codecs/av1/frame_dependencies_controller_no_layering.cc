/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/frame_dependencies_controller_no_layering.h"

#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"

namespace webrtc {

FrameDependenciesControllerNoLayering::
    ~FrameDependenciesControllerNoLayering() = default;

FrameDependencyStructure
FrameDependenciesControllerNoLayering::DependencyStructure() const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 0;
  return structure;
}

std::vector<GenericFrameInfo>
FrameDependenciesControllerNoLayering::NextFrameConfig(bool restart) {
  GenericFrameInfo info;
  info.is_keyframe = start_ || restart;
  info.encoder_buffers = {
      {/*id=*/0, /*references=*/!info.is_keyframe, /*updates=*/true}};
  start_ = false;
  return {info};
}

}  // namespace webrtc
