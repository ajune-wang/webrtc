/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_frame_metadata.h"

namespace webrtc {

VideoFrameMetadata::VideoFrameMetadata(const RTPVideoHeader& header)
    : header_(header) {
  SyncSuperflousStates();
}

std::map<std::string, std::string> VideoFrameMetadata::ToMap() const {
  return header_.ToMap();
}

bool VideoFrameMetadata::FromMap(
    const std::map<std::string, std::string>& map) {
  bool success = header_.FromMap(map);
  SyncSuperflousStates();
  return success;
}

void VideoFrameMetadata::SyncSuperflousStates() {
  width_ = header_.width;
  height_ = header_.height;
  if (header_.generic) {
    frame_id_ = header_.generic->frame_id;
    spatial_index_ = header_.generic->spatial_index;
    temporal_index_ = header_.generic->temporal_index;
    frame_dependencies_ = header_.generic->dependencies;
    decode_target_indications_ = header_.generic->decode_target_indications;
  }
}

}  // namespace webrtc
