/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/metronome_frame_scheduler.h"

#include <memory>

#include "rtc_base/checks.h"
namespace webrtc {

absl::optional<uint32_t>
MetronomeFrameDecodeScheduler::ScheduledRtpTimestamp() {
  return next_frame_.has_value() ? absl::make_optional(next_frame_->first)
                                 : absl::nullopt;
}

std::pair<uint32_t, FrameDecodeTiming::FrameSchedule>
MetronomeFrameDecodeScheduler::ReleaseNextFrame() {
  RTC_DCHECK(next_frame_);
  auto res = *next_frame_;
  next_frame_.reset();
  return res;
}

void MetronomeFrameDecodeScheduler::ScheduleFrame(
    uint32_t rtp,
    FrameDecodeTiming::FrameSchedule schedule) {
  RTC_DCHECK(!next_frame_) << "Can not schedule two frames at once.";
  next_frame_ = std::make_pair(rtp, std::move(schedule));
}

void MetronomeFrameDecodeScheduler::CancelOutstanding() {
  next_frame_.reset();
}

DecodeSyncronrizer::DecodeSyncronrizer(Clock* clock,
                                       Metronome* metronome,
                                       TaskQueueBase* worker_queue)
    : clock_(clock), worker_queue_(worker_queue), metronome_(metronome) {
  RTC_DCHECK(metronome_);
  RTC_DCHECK(worker_queue_);
}

DecodeSyncronrizer::~DecodeSyncronrizer() {
  RTC_DCHECK(receive_streams_.empty());
}

std::unique_ptr<FrameDecodeScheduler> DecodeSyncronrizer::AddReceiveStream(
    FrameDecodeScheduler::ReadyCallback* stream) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_DCHECK(stream);
  auto scheduler = std::make_unique<MetronomeFrameDecodeScheduler>();
  auto [it, inserted] = receive_streams_.emplace(stream, scheduler.get());
  RTC_DCHECK(inserted) << "Stream inserted twice!";

  if (receive_streams_.size() == 1) {
    metronome_->AddListener(this);
  }
  return std::move(scheduler);
}

void DecodeSyncronrizer::RemoveStream(
    FrameDecodeScheduler::ReadyCallback* stream,
    std::unique_ptr<FrameDecodeScheduler> scheduler) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_DCHECK(stream);
  auto it = receive_streams_.find(stream);
  if (it == receive_streams_.end()) {
    RTC_DCHECK_NOTREACHED()
        << "Attempted to remove a stream that was never inserted.";
    return;
  }
  RTC_DCHECK_EQ(it->second, scheduler.get())
      << "Removed stream but returned wrong scheduler!.";
  receive_streams_.erase(it);
  if (receive_streams_.empty()) {
    metronome_->RemoveListener(this);
  }
}

void DecodeSyncronrizer::OnTick() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  const Timestamp next_tick = clock_->CurrentTime() + metronome_->TickPeriod();
  for (auto& [callback, scheduler] : receive_streams_) {
    if (scheduler->ScheduledRtpTimestamp()) {
      auto [rtp, schedule] = scheduler->ReleaseNextFrame();
      if (schedule.max_decode_time < next_tick) {
        // If max decode time is before next tick, decode now.
        callback->FrameReadyForDecode(rtp, schedule.render_time);
      } else {
        // Otherwise, reschedule the frame for decoding later.
        scheduler->ScheduleFrame(rtp, schedule);
      }
    }
  }
}

TaskQueueBase* DecodeSyncronrizer::OnTickTaskQueue() {
  return worker_queue_;
}

}  // namespace webrtc
