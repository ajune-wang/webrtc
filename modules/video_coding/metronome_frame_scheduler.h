/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_METRONOME_FRAME_SCHEDULER_H_
#define MODULES_VIDEO_CODING_METRONOME_FRAME_SCHEDULER_H_

#include <map>
#include <memory>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/frame_buffer3.h"
#include "modules/video_coding/frame_scheduler.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

// Checks a number of frame schedulers on tick.
class MetronomeFrameScheduler {
 public:
  // TODO(eshr): Use OnTick.
  MetronomeFrameScheduler(Clock* clock, TaskQueueBase* worker_queue);
  MetronomeFrameScheduler(const MetronomeFrameScheduler&) = delete;
  MetronomeFrameScheduler& operator=(const MetronomeFrameScheduler&) = delete;

  void StartSchedulingFrames(FrameBuffer* frame_buffer,
                             FrameScheduler::Timeouts timeouts,
                             VCMTiming* timing,
                             FrameScheduler::Callback* callbacks);
  void StopSchedulingFrames(FrameBuffer* frame_buffer);
  void ForceKeyFrame(FrameBuffer* frame_buffer);
  void OnReceiverReady(FrameBuffer* frame_buffer);

 private:
  class VideoReceiver {
   public:
    VideoReceiver(FrameBuffer* frame_buffer,
                  FrameScheduler::Timeouts timeouts,
                  FrameScheduler::Callback* callbacks,
                  VCMTiming* timing,
                  Timestamp now);

    void OnTick(Timestamp now);
    void ForceKeyFrame();
    void OnReceiverReady();
    TimeDelta MaxWaitForFrame() const;

   private:
    absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> CheckForNewFrame(
        Timestamp now);
    FrameBuffer* const frame_buffer_;
    const FrameScheduler::Timeouts timeouts;
    FrameScheduler::Callback* const callbacks_;
    VCMTiming* timing;
    Timestamp last_released_frame_time_;
    bool requires_keyframe_;
    bool receiver_ready_ = false;
  };

  void OnTick();

  Clock* const clock_;
  TaskQueueBase* const worker_queue_;
  std::map<FrameBuffer*, VideoReceiver> receivers_
      RTC_GUARDED_BY(worker_queue_);
  // TODO(eshr): Use OnTick.
  RepeatingTaskHandle metronome_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_METRONOME_FRAME_SCHEDULER_H_
