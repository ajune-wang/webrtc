/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_FRAME_SMOOTHING_INHIBITOR_H_
#define VIDEO_FRAME_SMOOTHING_INHIBITOR_H_

#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"

namespace webrtc {

// Class for normally forwarding frames received on OnFrame on the smoothing
// sink, and conditionally on the direct sink
class FrameSmoothingInhibitor : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  FrameSmoothingInhibitor(rtc::VideoSinkInterface<VideoFrame>* smoothing_sink,
                          rtc::VideoSinkInterface<VideoFrame>* direct_sink);

  // If true is passed in |enabled|, incoming frames are forwarded on the
  // smoothing sink. Otherwise, incoming frames are forwarded to the direct
  // sink.
  void SetSmoothingEnabled(bool enabled);

 private:
  friend class FrameSmoothingInhibitorTest;
  void OnFrame(const VideoFrame& video_frame) override;

  rtc::VideoSinkInterface<VideoFrame>* smoothing_sink_;
  rtc::VideoSinkInterface<VideoFrame>* direct_sink_;
  bool smoothing_enabled_;
};

}  // namespace webrtc

#endif  // VIDEO_FRAME_SMOOTHING_INHIBITOR_H_
