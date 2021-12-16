/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/metronome_frame_scheduler.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

constexpr TimeDelta kTick = 1 / Frequency::Hertz(60);
constexpr TimeDelta kMaxAllowedFrameDelay = TimeDelta::Millis(5);

}  // namespace

MetronomeFrameScheduler::VideoReceiver::VideoReceiver(
    FrameBuffer* frame_buffer,
    FrameScheduler::Timeouts timeouts,
    FrameScheduler::Callback* callbacks,
    VCMTiming* timing,
    Timestamp now)
    : frame_buffer_(frame_buffer),
      timeouts(timeouts),
      callbacks_(callbacks),
      timing(timing),
      last_released_frame_time_(now),
      requires_keyframe_(true) {
  RTC_DCHECK(frame_buffer);
  RTC_DCHECK(callbacks);
  RTC_DCHECK(timing);
}

TimeDelta MetronomeFrameScheduler::VideoReceiver::MaxWaitForFrame() const {
  return requires_keyframe_ ? timeouts.max_wait_for_keyframe
                            : timeouts.max_wait_for_frame;
}

void MetronomeFrameScheduler::VideoReceiver::OnTick(Timestamp now) {
  if (!receiver_ready_) {
    return;
  }

  auto frames = CheckForNewFrame(now);
  // No frame found. Check timeout.
  if (frames.empty()) {
    if (now > last_released_frame_time_ + MaxWaitForFrame())
      callbacks_->OnTimeout();
    return;
  }

  requires_keyframe_ = false;
  receiver_ready_ = false;
  last_released_frame_time_ = now;
  callbacks_->OnFrameReady(std::move(frames));
}

absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4>
MetronomeFrameScheduler::VideoReceiver::CheckForNewFrame(Timestamp now) {
  if (requires_keyframe_) {
    while (frame_buffer_->NextDecodableTemporalUnitRtpTimestamp()) {
      auto frames = frame_buffer_->ExtractNextDecodableTemporalUnit();
      RTC_DCHECK(!frames.empty());
      if (frames.front()->is_keyframe()) {
        return frames;
      }
    }
    return {};
  }

  while (frame_buffer_->NextDecodableTemporalUnitRtpTimestamp().has_value()) {
    auto next_rtp = *frame_buffer_->NextDecodableTemporalUnitRtpTimestamp();
    // TODO(eshr): Check not decodable.

    // Current frame with given rtp might be decodable.
    int64_t render_time = timing->RenderTimeMs(next_rtp, now.ms());
    TimeDelta max_wait =
        TimeDelta::Millis(timing->MaxWaitingTime(render_time, now.ms(), false));

    // If the delay is not too far in the past, or this is the last decodable
    // frame then it is the best frame to be decoded. Otherwise, fast-forward
    // to the next frame in the buffer.
    if (max_wait > -kMaxAllowedFrameDelay ||
        next_rtp == frame_buffer_->LastDecodableTemporalUnitRtpTimestamp()) {
      RTC_DLOG_V(rtc::LS_VERBOSE)
          << __FUNCTION__ << " selected frame with rtp " << next_rtp
          << " render time " << render_time << " with a max wait of "
          << max_wait.ms() << "ms";
      auto frames = frame_buffer_->ExtractNextDecodableTemporalUnit();
      for (auto& frame : frames) {
        frame->SetRenderTime(render_time);
      }

      return frames;
    }
    RTC_DLOG_V(rtc::LS_VERBOSE) << __FUNCTION__ << " fast-forwarded frame "
                                << next_rtp << " render time " << render_time
                                << " with delay " << max_wait.ms() << "ms";
    frame_buffer_->DropNextDecodableTemporalUnit();
  }

  RTC_DLOG_V(rtc::LS_VERBOSE)
      << __FUNCTION__ << " selected no frame frame to decode.";
  return {};
}

void MetronomeFrameScheduler::VideoReceiver::OnReceiverReady() {
  receiver_ready_ = true;
}

void MetronomeFrameScheduler::VideoReceiver::ForceKeyFrame() {
  requires_keyframe_ = true;
}

MetronomeFrameScheduler::MetronomeFrameScheduler(Clock* clock,
                                                 TaskQueueBase* worker_queue)
    : clock_(clock), worker_queue_(worker_queue) {
  RTC_DCHECK(clock_);
  RTC_DCHECK(worker_queue_);
}

void MetronomeFrameScheduler::StartSchedulingFrames(
    FrameBuffer* frame_buffer,
    FrameScheduler::Timeouts timeouts,
    VCMTiming* timing,
    FrameScheduler::Callback* callbacks) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  bool inserted;
  std::tie(std::ignore, inserted) = receivers_.emplace(std::make_pair(
      frame_buffer, VideoReceiver(frame_buffer, timeouts, callbacks, timing,
                                  clock_->CurrentTime())));
  RTC_DCHECK(inserted)
      << "Not allowed to schedule frames twice on the same frame buffer";

  // If this is the first one - start the metronome.
  if (receivers_.size() == 1) {
    RTC_DCHECK(!metronome_.Running());
    metronome_.DelayedStart(worker_queue_, kTick, [this] {
      OnTick();
      return kTick;
    });
  }
}

void MetronomeFrameScheduler::StopSchedulingFrames(FrameBuffer* frame_buffer) {
  RTC_DCHECK_RUN_ON(worker_queue_);

  auto it = receivers_.find(frame_buffer);
  if (it == receivers_.end()) {
    RTC_DCHECK_NOTREACHED()
        << "Was not listening on a frame buffer that was stopped.";
    return;
  }

  receivers_.erase(it);

  if (receivers_.empty()) {
    metronome_.Stop();
  }
}

void MetronomeFrameScheduler::ForceKeyFrame(FrameBuffer* frame_buffer) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  auto it = receivers_.find(frame_buffer);
  if (it == receivers_.end()) {
    RTC_DCHECK_NOTREACHED()
        << "Was not listening on a frame buffer that was stopped.";
    return;
  }
  it->second.ForceKeyFrame();
}

void MetronomeFrameScheduler::OnReceiverReady(FrameBuffer* frame_buffer) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  auto it = receivers_.find(frame_buffer);
  if (it == receivers_.end()) {
    RTC_DCHECK_NOTREACHED()
        << "Was not listening on a frame buffer that was stopped.";
    return;
  }
  it->second.OnReceiverReady();
}

void MetronomeFrameScheduler::OnTick() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  Timestamp now = clock_->CurrentTime();
  for (auto& it : receivers_) {
    it.second.OnTick(now);
  }
}

}  // namespace webrtc
