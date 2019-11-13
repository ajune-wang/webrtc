/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_smoothing_inhibitor.h"

namespace webrtc {

FrameSmoothingInhibitor::FrameSmoothingInhibitor(
    rtc::VideoSinkInterface<VideoFrame>* smoothing_sink,
    rtc::VideoSinkInterface<VideoFrame>* direct_sink)
    : smoothing_sink_(smoothing_sink),
      direct_sink_(direct_sink),
      smoothing_enabled_(true) {}

void FrameSmoothingInhibitor::SetSmoothingEnabled(bool enabled) {
  smoothing_enabled_ = enabled;
}

void FrameSmoothingInhibitor::OnFrame(const VideoFrame& video_frame) {
  rtc::VideoSinkInterface<VideoFrame>* sink =
      smoothing_enabled_ ? smoothing_sink_ : direct_sink_;
  sink->OnFrame(video_frame);
}

}  // namespace webrtc
