/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_FRAME_DECODE_SCHEDULER_H_
#define VIDEO_FRAME_DECODE_SCHEDULER_H_

#include <stdint.h>

#include <functional>

#include "absl/types/optional.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/timestamp.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "system_wrappers/include/clock.h"
#include "video/frame_decode_timing.h"

namespace webrtc {

class FrameDecodeScheduler {
 public:
  virtual ~FrameDecodeScheduler() = default;
  // Invoked when a frame with `rtp_timestamp` is ready for decoding.
  class ReadyCallback {
   public:
    virtual ~ReadyCallback() = default;
    virtual void FrameReadyForDecode(uint32_t rtp_timestamp,
                                     Timestamp redner_time) = 0;
  };

  virtual absl::optional<uint32_t> ScheduledRtpTimestamp() = 0;

  virtual void ScheduleFrame(uint32_t rtp,
                             FrameDecodeTiming::FrameSchedule schedule) = 0;
  virtual void CancelOutstanding() = 0;
};

class TaskQueueFrameDecodeScheduler : public FrameDecodeScheduler {
 public:
  // Invoked when a frame with `rtp_timestamp` is ready for decoding.
  TaskQueueFrameDecodeScheduler(Clock* clock,
                                TaskQueueBase* const bookkeeping_queue,
                                ReadyCallback* callback);
  ~TaskQueueFrameDecodeScheduler() override;
  TaskQueueFrameDecodeScheduler(const TaskQueueFrameDecodeScheduler&) = delete;
  TaskQueueFrameDecodeScheduler& operator=(
      const TaskQueueFrameDecodeScheduler&) = delete;

  absl::optional<uint32_t> ScheduledRtpTimestamp() override;

  void ScheduleFrame(uint32_t rtp,
                     FrameDecodeTiming::FrameSchedule schedule) override;
  void CancelOutstanding() override;

 private:
  Clock* const clock_;
  TaskQueueBase* const bookkeeping_queue_;
  ReadyCallback* const callback_;

  absl::optional<uint32_t> scheduled_rtp_;
  ScopedTaskSafetyDetached task_safety_;
};

}  // namespace webrtc

#endif  // VIDEO_FRAME_DECODE_SCHEDULER_H_
