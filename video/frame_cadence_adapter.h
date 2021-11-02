/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_FRAME_CADENCE_ADAPTER_H_
#define VIDEO_FRAME_CADENCE_ADAPTER_H_

#include <memory>

#include "api/units/time_delta.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

// A sink adapter implementing mutations to the received frame cadence.
// With the exception of construction & destruction which has to happen on the
// same sequence, this class is thread-safe because three different execution
// contexts call into it.
class FrameCadenceAdapterInterface
    : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  // Averaging window spanning 90 frames at default 30fps, matching old media
  // optimization module defaults.
  static constexpr int64_t kFrameRateAvergingWindowSizeMs = (1000 / 30) * 90;

  // Callback interface used to inform instance owners.
  class Callback {
   public:
    virtual ~Callback() = default;

    // Called when a frame arrives. If set, the |duration| specifies the minimum
    // time until the next frame comes.
    virtual void OnFrame(const VideoFrame& frame,
                         absl::optional<TimeDelta> duration) = 0;

    // Called when the source has discarded a frame.
    virtual void OnDiscardedFrame() = 0;
  };

  // Factory function creating a production instance. Deletion of the returned
  // instance needs to happen on the same sequence that Create() was called on.
  static std::unique_ptr<FrameCadenceAdapterInterface> Create(Clock* clock);

  // Call before using the rest of the API.
  virtual void Initialize(Callback* callback) = 0;

  // Pass true in |enabled| when the content type allows.
  virtual void SetEnabledByContentType(bool enabled) = 0;

  // Returns the input framerate. This is measured by RateStatistics when
  // zero-hertz mode is off, and returns the max framerate in zero-hertz mode.
  virtual absl::optional<uint32_t> GetInputFramerateFps() = 0;

  // Updates frame rate. This is done unconditionally irrespective of adapter
  // mode.
  virtual void UpdateFrameRate() = 0;
};

}  // namespace webrtc

#endif  // VIDEO_FRAME_CADENCE_ADAPTER_H_
