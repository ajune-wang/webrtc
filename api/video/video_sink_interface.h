/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_SINK_INTERFACE_H_
#define API_VIDEO_VIDEO_SINK_INTERFACE_H_

#include <vector>

#include "api/video/video_frame.h"
#include "rtc_base/checks.h"

namespace rtc {

template <typename VideoFrameT>
class VideoSinkInterface {
 public:
  virtual ~VideoSinkInterface() = default;

  // Called when a new frame is delivered to the sink. |video_frames| must
  // contain at least one frame but may optionally contain additional, scaled
  // versions of the original frame. When multiple versions of the frame is
  // delivered, video_frames[0] must be the largest one. Additional frames can
  // be in any order.
  //
  // The adapted source size is the size of video frames produced by the source
  // after downscaling due to adaptation. For example, a source that is
  // capturing in 720p may be limited to 480p due to bandwidth constraints, in
  // which case the adapted source size is 480p.
  //
  // video_frames[0] must not be larger than adapted source size, but it may in
  // some cases be smaller if the source has been configured to deliver
  // downscaled versions of the adapted source. For example if the sink is a
  // VideoStreamEncoder, we may capture at 720p, bandwidth limit it to 480p and
  // then encode it at 360p due to scaleResolutionDownBy usage. In this case if
  // video_frame[0] is already 360p then the VideoStreamEncoder can skip
  // downscaling from 480p to 360p.
  // TODO(https://crbug.com/webrtc/12469): Implement not having to downscale to
  // 480p before encoding at 360p in the example provided above.
  //
  // TODO(https://crbug.com/1157072): When upstream projects implement this
  // signature, remove the default implementation.
  virtual void OnFrame(int adapted_source_width,
                       int adapted_source_height,
                       const std::vector<const VideoFrameT*>& video_frames) {
    RTC_DCHECK(!video_frames.empty());
    RTC_DCHECK_GE(adapted_source_width, video_frames[0]->width());
    RTC_DCHECK_GE(adapted_source_height, video_frames[0]->height());
    OnFrame(*video_frames[0]);
  }
  // A default version of the old signature to allow migrating to the new
  // signature.
  // TODO(https://crbug.com/1157072): When upstream projects have migrated
  // signature, delete this version of OnFrame().
  virtual void OnFrame(const VideoFrameT& frame) {
    OnFrame(frame.width(), frame.height(), {&frame});
  }

  // Should be called by the source when it discards the frame due to rate
  // limiting.
  virtual void OnDiscardedFrame() {}
};

}  // namespace rtc

#endif  // API_VIDEO_VIDEO_SINK_INTERFACE_H_
