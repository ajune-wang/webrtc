/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_source_interface.h"

namespace rtc {

VideoSinkWants::VideoSinkWants() = default;
VideoSinkWants::VideoSinkWants(const VideoSinkWants&) = default;
VideoSinkWants::~VideoSinkWants() = default;
bool VideoSinkWants::operator==(const VideoSinkWants& rhs) const {
  return std::tie(rotation_applied, black_frames, max_pixel_count,
                  target_pixel_count, max_framerate_fps, resolution_alignment,
                  resolutions) ==
         std::tie(rhs.rotation_applied, rhs.black_frames, rhs.max_pixel_count,
                  rhs.target_pixel_count, rhs.max_framerate_fps,
                  rhs.resolution_alignment, rhs.resolutions);
}

}  // namespace rtc
