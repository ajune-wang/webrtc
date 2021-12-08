/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_buffer2_adapter.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_image.h"
#include "api/video/video_timing.h"
#include "modules/video_coding/frame_buffer3.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/jitter_estimator.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/rtt_mult_experiment.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

constexpr size_t kMaxFramesBuffered = 800;
constexpr int kMaxFramesHistory = 1 << 13;
struct StatsClosure {
  StatsClosure(const FrameBuffer* frame_buffer,
               VCMReceiveStatisticsCallback* stats_callback)
      : stats_callback(stats_callback),
        frame_buffer(frame_buffer),
        start(frame_buffer->GetTotalNumberOfDroppedFrames()) {}
  StatsClosure(const StatsClosure&) = delete;
  StatsClosure& operator=(const StatsClosure&) = delete;

  ~StatsClosure() {
    if (!stats_callback)
      return;
    int diff = frame_buffer->GetTotalNumberOfDroppedFrames() - start;
    if (diff != 0)
      stats_callback->OnDroppedFrames(diff);
  }

  VCMReceiveStatisticsCallback* const stats_callback;
  const FrameBuffer* const frame_buffer;
  const int start;
};

}  // namespace

FrameBuffer2Adapter::FrameBuffer2Adapter(
    DecodeStreamTimeouts timeouts,
    Clock* clock,
    VCMTiming* timing,
    VCMReceiveStatisticsCallback* stats_callback)
    : timeouts_(timeouts),
      clock_(clock),
      frame_buffer_(kMaxFramesBuffered, kMaxFramesHistory),
      timing_(timing),
      jitter_estimator_(clock),
      inter_frame_delay_(clock_->TimeInMilliseconds()),
      protection_mode_(kProtectionNack),
      stats_callback_(stats_callback),
      rtt_mult_settings_(RttMultExperiment::GetRttMultValue()) {}

FrameBuffer2Adapter::~FrameBuffer2Adapter() {
  RTC_DCHECK(!scheduler_);
}

void FrameBuffer2Adapter::NextFrame(
    bool keyframe_required,
    rtc::TaskQueue* callback_queue,
    FrameBuffer2Interface::NextFrameCallback handler) {
  RTC_DCHECK(callback_queue->IsCurrent());
  TRACE_EVENT0("webrtc", "FrameBuffer::NextFrame");

  MutexLock lock(&mutex_);
  if (!callback_queue_) {
    // First call to NextFrame - setup handlers.
    callback_queue_ = callback_queue;
    RTC_DCHECK(!scheduler_);
    scheduler_ = std::make_unique<FrameScheduler>(
        clock_, callback_queue_, timing_, &frame_buffer_, timeouts_,
        absl::bind_front(&FrameBuffer2Adapter::OnFrameReady, this),
        absl::bind_front(&FrameBuffer2Adapter::OnTimeout, this));
    // TODO: Is this needed?
    if (keyframe_required)
      scheduler_->ForceKeyFrame();
    scheduler_->Start();
  } else {
    RTC_DCHECK(callback_queue_ == callback_queue)
        << "Switching callback_queue_ is not supported.";
  }

  // See if we have stopped.
  if (!scheduler_) {
    return;
  }
  if (keyframe_required)
    scheduler_->ForceKeyFrame();
  frame_handler_ = handler;
}

std::unique_ptr<EncodedFrame> FrameBuffer2Adapter::CombineFrames(
    absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames_to_decode,
    int64_t render_time_ms) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  RTC_DCHECK(!frames_to_decode.empty());
  bool superframe_delayed_by_retransmission = false;
  size_t superframe_size = 0;
  const EncodedFrame& first_frame = *frames_to_decode.front();
  int64_t receive_time_ms = first_frame.ReceivedTime();
  // Gracefully handle bad RTP timestamps and render time issues.
  if (HasBadRenderTiming(render_time_ms, now_ms)) {
    jitter_estimator_.Reset();
    timing_->Reset();
    render_time_ms = timing_->RenderTimeMs(first_frame.Timestamp(), now_ms);
  }

  for (std::unique_ptr<EncodedFrame>& frame : frames_to_decode) {
    frame->SetRenderTime(render_time_ms);

    superframe_delayed_by_retransmission |= frame->delayed_by_retransmission();
    receive_time_ms = std::max(receive_time_ms, frame->ReceivedTime());
    superframe_size += frame->size();
  }

  if (!superframe_delayed_by_retransmission) {
    int64_t frame_delay;

    if (inter_frame_delay_.CalculateDelay(first_frame.Timestamp(), &frame_delay,
                                          receive_time_ms)) {
      jitter_estimator_.UpdateEstimate(frame_delay, superframe_size);
    }

    float rtt_mult = protection_mode_ == kProtectionNackFEC ? 0.0 : 1.0;
    absl::optional<float> rtt_mult_add_cap_ms = absl::nullopt;
    if (rtt_mult_settings_.has_value()) {
      rtt_mult = rtt_mult_settings_->rtt_mult_setting;
      rtt_mult_add_cap_ms = rtt_mult_settings_->rtt_mult_add_cap_ms;
    }
    timing_->SetJitterDelay(
        jitter_estimator_.GetJitterEstimate(rtt_mult, rtt_mult_add_cap_ms));
    timing_->UpdateCurrentDelay(render_time_ms, now_ms);
  } else if (RttMultExperiment::RttMultEnabled()) {
    jitter_estimator_.FrameNacked();
  }

  if (frames_to_decode.size() == 1) {
    return std::move(frames_to_decode[0]);
  } else {
    return CombineAndDeleteFrames(std::move(frames_to_decode));
  }
}

bool FrameBuffer2Adapter::HasBadRenderTiming(int64_t render_time_ms,
                                             int64_t now_ms) const {
  // Zero render time means render immediately.
  if (render_time_ms == 0) {
    return false;
  }
  if (render_time_ms < 0) {
    return true;
  }
  const int64_t kMaxVideoDelayMs = 10000;
  if (std::abs(render_time_ms - now_ms) > kMaxVideoDelayMs) {
    int frame_delay = static_cast<int>(std::abs(render_time_ms - now_ms));
    RTC_LOG(LS_WARNING)
        << "A frame about to be decoded is out of the configured "
           "delay bounds ("
        << frame_delay << " > " << kMaxVideoDelayMs
        << "). Resetting the video jitter buffer.";
    return true;
  }
  if (static_cast<int>(timing_->TargetVideoDelay()) > kMaxVideoDelayMs) {
    RTC_LOG(LS_WARNING) << "The video target delay has grown larger than "
                        << kMaxVideoDelayMs << " ms.";
    return true;
  }
  return false;
}

void FrameBuffer2Adapter::SetProtectionMode(VCMVideoProtection mode) {
  TRACE_EVENT0("webrtc", "FrameBuffer::SetProtectionMode");
  MutexLock lock(&mutex_);
  protection_mode_ = mode;
}

void FrameBuffer2Adapter::Stop() {
  TRACE_EVENT0("webrtc", "FrameBuffer::Stop");
  MutexLock lock(&mutex_);
  if (scheduler_)
    scheduler_->Stop();
  scheduler_ = nullptr;
  callback_queue_ = nullptr;
  frame_handler_ = {};
}

void FrameBuffer2Adapter::Clear() {
  MutexLock lock(&mutex_);
  ClearFramesAndHistory();
}

int FrameBuffer2Adapter::Size() {
  MutexLock lock(&mutex_);
  return frame_buffer_.Size();
}

void FrameBuffer2Adapter::UpdateRtt(int64_t rtt_ms) {
  MutexLock lock(&mutex_);
  jitter_estimator_.UpdateRtt(rtt_ms);
}

int64_t FrameBuffer2Adapter::InsertFrame(std::unique_ptr<EncodedFrame> frame) {
  TRACE_EVENT0("webrtc", "FrameBuffer::InsertFrame");
  RTC_DCHECK(frame);

  MutexLock lock(&mutex_);
  // TODO(eshr): Replace complete frame
  if (stats_callback_ && frame->is_last_spatial_layer) {
    stats_callback_->OnCompleteFrame(frame->is_keyframe(), frame->size(),
                                     frame->contentType());
  }
  if (!frame->delayed_by_retransmission())
    timing_->IncomingTimestamp(frame->Timestamp(), frame->ReceivedTime());

  frame_buffer_.InsertFrame(std::move(frame));

  if (scheduler_)
    scheduler_->OnNewFrameInserted();

  return frame_buffer_.LastContinuousFrameId().value_or(-1);
}

void FrameBuffer2Adapter::UpdateJitterDelay() {
  TRACE_EVENT0("webrtc", "FrameBuffer::UpdateJitterDelay");
  if (!stats_callback_)
    return;

  int max_decode_ms;
  int current_delay_ms;
  int target_delay_ms;
  int jitter_buffer_ms;
  int min_playout_delay_ms;
  int render_delay_ms;
  if (timing_->GetTimings(&max_decode_ms, &current_delay_ms, &target_delay_ms,
                          &jitter_buffer_ms, &min_playout_delay_ms,
                          &render_delay_ms)) {
    stats_callback_->OnFrameBufferTimingsUpdated(
        max_decode_ms, current_delay_ms, target_delay_ms, jitter_buffer_ms,
        min_playout_delay_ms, render_delay_ms);
  }
}

void FrameBuffer2Adapter::UpdateTimingFrameInfo() {
  TRACE_EVENT0("webrtc", "FrameBuffer::UpdateTimingFrameInfo");
  absl::optional<TimingFrameInfo> info = timing_->GetTimingFrameInfo();
  if (info && stats_callback_)
    stats_callback_->OnTimingFrameInfoUpdated(*info);
}

void FrameBuffer2Adapter::ClearFramesAndHistory() {
  TRACE_EVENT0("webrtc", "FrameBuffer::ClearFramesAndHistory");
  if (stats_callback_ && scheduler_) {
    stats_callback_->OnDroppedFrames(frame_buffer_.Size());
  }
  if (scheduler_) {
    scheduler_->Stop();
    scheduler_ = nullptr;
    callback_queue_ = nullptr;
    frame_handler_ = {};
  }
  frame_buffer_.Clear();
}

void FrameBuffer2Adapter::OnTimeout() {
  MutexLock lock(&mutex_);
  frame_handler_(nullptr);
}

void FrameBuffer2Adapter::OnFrameReady(
    absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames) {
  RTC_DCHECK(!frames.empty());
  MutexLock lock(&mutex_);
  UpdateJitterDelay();
  UpdateTimingFrameInfo();

  const auto& first_frame = frames.front();
  frame_handler_(CombineFrames(std::move(frames), first_frame->RenderTimeMs()));
}

// TODO(philipel): Avoid the concatenation of frames here, by replacing
// NextFrame and GetNextFrame with methods returning multiple frames.
std::unique_ptr<EncodedFrame> FrameBuffer2Adapter::CombineAndDeleteFrames(
    absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames) const {
  RTC_DCHECK(!frames.empty());
  size_t total_length = 0;
  for (const auto& frame : frames) {
    total_length += frame->size();
  }
  const EncodedFrame& last_frame = *frames.back();
  std::unique_ptr<EncodedFrame> first_frame = std::move(frames[0]);
  auto encoded_image_buffer = EncodedImageBuffer::Create(total_length);
  uint8_t* buffer = encoded_image_buffer->data();
  first_frame->SetSpatialLayerFrameSize(first_frame->SpatialIndex().value_or(0),
                                        first_frame->size());
  memcpy(buffer, first_frame->data(), first_frame->size());
  buffer += first_frame->size();

  // Spatial index of combined frame is set equal to spatial index of its top
  // spatial layer.
  first_frame->SetSpatialIndex(last_frame.SpatialIndex().value_or(0));

  first_frame->video_timing_mutable()->network2_timestamp_ms =
      last_frame.video_timing().network2_timestamp_ms;
  first_frame->video_timing_mutable()->receive_finish_ms =
      last_frame.video_timing().receive_finish_ms;

  // Append all remaining frames to the first one.
  for (size_t i = 1; i < frames.size(); ++i) {
    // Let |next_frame| fall out of scope so it is deleted after copying.
    std::unique_ptr<EncodedFrame> next_frame = std::move(frames[i]);
    first_frame->SetSpatialLayerFrameSize(
        next_frame->SpatialIndex().value_or(0), next_frame->size());
    memcpy(buffer, next_frame->data(), next_frame->size());
    buffer += next_frame->size();
  }
  first_frame->SetEncodedData(encoded_image_buffer);
  return first_frame;
}

}  // namespace webrtc
