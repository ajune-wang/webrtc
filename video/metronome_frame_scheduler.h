/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_METRONOME_FRAME_SCHEDULER_H_
#define VIDEO_METRONOME_FRAME_SCHEDULER_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "api/metronome/metronome.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/checks.h"
#include "rtc_base/thread_annotations.h"
#include "video/frame_decode_scheduler.h"
#include "video/frame_decode_timing.h"

namespace webrtc {

class MetronomeFrameDecodeScheduler : public FrameDecodeScheduler {
 public:
  absl::optional<uint32_t> ScheduledRtpTimestamp() override;
  void ScheduleFrame(uint32_t rtp,
                     FrameDecodeTiming::FrameSchedule schedule) override;
  void CancelOutstanding() override;

  std::pair<uint32_t, FrameDecodeTiming::FrameSchedule> ReleaseNextFrame();

 private:
  absl::optional<std::pair<uint32_t, FrameDecodeTiming::FrameSchedule>>
      next_frame_;
};

class DecodeSyncronrizer : private Metronome::TickListener {
 public:
  DecodeSyncronrizer(Clock* clock,
                     Metronome* metronome,
                     TaskQueueBase* worker_queue);

  ~DecodeSyncronrizer();

  std::unique_ptr<FrameDecodeScheduler> AddReceiveStream(
      FrameDecodeScheduler::ReadyCallback* stream);

  void RemoveStream(FrameDecodeScheduler::ReadyCallback* stream,
                    std::unique_ptr<FrameDecodeScheduler> scheduler);

 private:
  // Metronome::TickListener implementation.
  void OnTick() override;
  TaskQueueBase* OnTickTaskQueue() override;

  Clock* const clock_;
  TaskQueueBase* const worker_queue_;
  Metronome* const metronome_;
  std::map<FrameDecodeScheduler::ReadyCallback*, MetronomeFrameDecodeScheduler*>
      receive_streams_ RTC_GUARDED_BY(worker_queue_);
};

}  // namespace webrtc

#endif  // VIDEO_METRONOME_FRAME_SCHEDULER_H_
