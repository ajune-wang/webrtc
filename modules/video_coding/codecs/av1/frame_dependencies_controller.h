/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_H_

#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"

namespace webrtc {

class FrameDependenciesController {
 public:
  virtual ~FrameDependenciesController() = default;

  virtual FrameDependencyStructure DependencyStructure() const = 0;
  // Returns list of configurations the frame should be encoded with.
  // Returns empty list to indicate frame should be dropped.
  // Normally returns one configuration per active spatial layer.
  // When `restart` is true, first GenericFrameInfo should have `is_keyframe`
  // set to true
  virtual std::vector<GenericFrameInfo> NextFrameConfig(bool restart) = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_H_
