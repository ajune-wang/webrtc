/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/frame_dependencies_controller_single_stream.h"

#include <utility>

#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "rtc_base/checks.h"

namespace webrtc {

FrameDependencyStructure
FrameDependenciesControllerSingleStream::DependencyStructure() const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 1;
  structure.num_chains = 0;
  if (max_resolution_) {
    structure.resolutions.push_back(*max_resolution_);
  }
  FrameDependencyTemplate key_frame;
  key_frame.decode_target_indications = {DecodeTargetIndication::kSwitch};
  FrameDependencyTemplate delta_frame;
  delta_frame.decode_target_indications = {DecodeTargetIndication::kSwitch};
  delta_frame.frame_diffs = {1};
  structure.templates = {std::move(key_frame), std::move(delta_frame)};
  return structure;
}

std::vector<GenericFrameInfo>
FrameDependenciesControllerSingleStream::NextFrameConfig(bool reset) {
  if (reset) {
    beginning_of_stream_ = true;
  }
  GenericFrameInfo result;
  result.decode_target_indications = {DecodeTargetIndication::kSwitch};
  result.encoder_buffers = {CodecBufferUsage(
      /*id=*/0, /*reference=*/!beginning_of_stream_, /*updates=*/true)};
  beginning_of_stream_ = false;
  return {result};
}

}  // namespace webrtc
