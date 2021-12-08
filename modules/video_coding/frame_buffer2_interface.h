/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef _MODULES_VIDEO_CODING_FRAME_BUFFER2_INTERFACE_H_
#define _MODULES_VIDEO_CODING_FRAME_BUFFER2_INTERFACE_H_

#include <memory>
#include <utility>

#include "api/video/encoded_frame.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/task_queue.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

struct DecodeStreamTimeouts {
  TimeDelta max_wait_for_keyframe;
  TimeDelta max_wait_for_frame;

  TimeDelta MaxWait(bool keyframe) const {
    return keyframe ? max_wait_for_keyframe : max_wait_for_frame;
  }
};

class FrameBuffer2Interface {
 public:
  virtual ~FrameBuffer2Interface() = default;

  // Insert a frame into the frame buffer. Returns the picture id
  // of the last continuous frame or -1 if there is no continuous frame.
  virtual int64_t InsertFrame(std::unique_ptr<EncodedFrame> frame) = 0;

  // Get the next frame for decoding. Will return at latest after
  // `max_wait_time_ms`.
  using NextFrameCallback = std::function<void(std::unique_ptr<EncodedFrame>)>;
  virtual void NextFrame(bool keyframe_required,
                         rtc::TaskQueue* callback_queue,
                         NextFrameCallback handler) = 0;

  // Tells the FrameBuffer which protection mode that is in use. Affects
  // the frame timing.
  // TODO(philipel): Remove this when new timing calculations has been
  //                 implemented.
  virtual void SetProtectionMode(VCMVideoProtection mode) = 0;

  // Stop the frame buffer, causing any sleeping thread in NextFrame to
  // return immediately.
  virtual void Stop() = 0;

  // Updates the RTT for jitter buffer estimation.
  virtual void UpdateRtt(int64_t rtt_ms) = 0;

  // Clears the FrameBuffer, removing all the buffered frames.
  virtual void Clear() = 0;

  virtual int Size() = 0;
};

std::unique_ptr<FrameBuffer2Interface> CreateFrameBuffer2FromFieldTrial(
    DecodeStreamTimeouts timeouts,
    Clock* clock,
    VCMTiming* timing,
    VCMReceiveStatisticsCallback* stats_callback);

}  // namespace webrtc

#endif  // _MODULES_VIDEO_CODING_FRAME_BUFFER2_INTERFACE_H_