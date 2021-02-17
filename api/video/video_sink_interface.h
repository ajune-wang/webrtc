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

#include "rtc_base/checks.h"

namespace rtc {

template <typename VideoFrameT>
class VideoSinkInterface {
 public:
  virtual ~VideoSinkInterface() = default;

  // Called when a new frame is delivered to the sink. |frame| is the highest
  // resolution video frame available and |scaled_frames| may optionally contain
  // scaled versions of that frame.
  //
  // The adapted source size is the size that frames produced by the source
  // would have after adaptation scaling is applied. For example, we may want to
  // adapt a 720p captured frame to 480p due to BW limitations. However whether
  // we want to encode 480p or lower may depend on encoder settings, such as
  // scaleResolutionDownBy.
  //
  // The highest resolution frame is either the same size as the adapted source
  // size or it is the bigger, original version. This means that any frame size
  // can be downscaled from |frame| which is necessary if |scaled_frames| does
  // not contain the desired size.
  //
  // All frames in |scaled_frames| are smaller than the adapted source size. If
  // the sink desires a downscaled version of |frame| and |scaled_frames|
  // contains the desired size we can avoid the cost of downscaling here.
  //
  // TODO(https://crbug.com/1157072): When upstream projects implement this
  // signature, remove the default implementation.
  virtual void OnFrames(int adapted_source_width,
                        int adapted_source_height,
                        const VideoFrameT& frame,
                        std::vector<const VideoFrameT*> scaled_frames) {
    OnFrame(frame);
  }
  // A default version of the old signature to allow migrating to the new
  // signature.
  // TODO(https://crbug.com/1157072): When upstream projects have migrated
  // signature, delete this version of OnFrame().
  virtual void OnFrame(const VideoFrameT& frame) {
    OnFrames(frame.width(), frame.height(), frame, {});
  }

  // Should be called by the source when it discards the frame due to rate
  // limiting.
  virtual void OnDiscardedFrame() {}
};

}  // namespace rtc

#endif  // API_VIDEO_VIDEO_SINK_INTERFACE_H_
