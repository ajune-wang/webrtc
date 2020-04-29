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

// Interface to control video stream structure of an AV1 stream.
class FrameDependenciesController {
 public:
  virtual ~FrameDependenciesController() = default;

  virtual FrameDependencyStructure Structure() const = 0;
  // Returns a configuration per spatial layer for the next frame to encode.
  virtual std::vector<GenericFrameInfo> NextFrameConfig() = 0;
  // Updates frame dependencies after frame is encoded, if necessary.
  // Expects |frame_dependencies| to be not null and points to one returned by
  // NextFrameConfig().
  // Returns false on error.
  virtual bool OnEncodeDone(bool is_keyframe,
                            GenericFrameInfo* frame_dependencies) = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_FRAME_DEPENDENCIES_CONTROLLER_H_
