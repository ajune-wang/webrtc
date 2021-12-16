/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_buffer_proxy.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/types/variant.h"
#include "modules/video_coding/frame_buffer2.h"
#include "modules/video_coding/frame_buffer3.h"
#include "modules/video_coding/frame_helpers.h"
#include "modules/video_coding/frame_scheduler.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {
// From https://en.cppreference.com/w/cpp/utility/variant/visit
// helper type for the visitor #4
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

class FrameBuffer2Proxy : public FrameBufferProxy {
 public:
  FrameBuffer2Proxy(Clock* clock,
                    VCMTiming* timing,
                    VCMReceiveStatisticsCallback* stats_proxy,
                    rtc::TaskQueue* decode_queue,
                    FrameSchedulingReceiver* receiver,
                    TimeDelta max_wait_for_keyframe,
                    TimeDelta max_wait_for_frame)
      : max_wait_for_keyframe_(max_wait_for_keyframe),
        max_wait_for_frame_(max_wait_for_frame),
        frame_buffer_(clock, timing, stats_proxy),
        decode_queue_(decode_queue),
        stats_proxy_(stats_proxy),
        receiver_(receiver) {
    RTC_DCHECK(decode_queue_);
    RTC_DCHECK(stats_proxy_);
    RTC_DCHECK(receiver_);
  }

  void StopOnWorker() override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    decode_queue_->PostTask([this] {
      frame_buffer_.Stop();
      decode_safety_->SetNotAlive();
    });
  }

  void SetProtectionMode(VCMVideoProtection protection_mode) override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    frame_buffer_.SetProtectionMode(kProtectionNackFEC);
  }

  void Clear() override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    frame_buffer_.Clear();
  }

  absl::optional<int64_t> InsertFrame(
      std::unique_ptr<EncodedFrame> frame) override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    int64_t last_continuous_pid = frame_buffer_.InsertFrame(std::move(frame));
    if (last_continuous_pid != -1)
      return last_continuous_pid;
    return absl::nullopt;
  }

  void UpdateRtt(int64_t max_rtt_ms) override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    frame_buffer_.UpdateRtt(max_rtt_ms);
  }

  void StartNextDecode(bool keyframe_required) override {
    if (!decode_queue_->IsCurrent()) {
      decode_queue_->PostTask(ToQueuedTask(
          decode_safety_,
          [this, keyframe_required] { StartNextDecode(keyframe_required); }));
      return;
    }
    RTC_DCHECK_RUN_ON(decode_queue_);

    frame_buffer_.NextFrame(
        MaxWait(keyframe_required).ms(), keyframe_required, decode_queue_,
        /* encoded frame handler */
        [this, keyframe_required](std::unique_ptr<EncodedFrame> frame) {
          RTC_DCHECK_RUN_ON(decode_queue_);
          if (!decode_safety_->alive())
            return;
          if (frame) {
            receiver_->OnEncodedFrame(std::move(frame));
          } else {
            receiver_->OnDecodableFrameTimeout(MaxWait(keyframe_required));
          }
        });
  }

  int Size() override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    return frame_buffer_.Size();
  }

 private:
  TimeDelta MaxWait(bool keyframe_required) const {
    return keyframe_required ? max_wait_for_keyframe_ : max_wait_for_frame_;
  }

  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_sequence_checker_;
  const TimeDelta max_wait_for_keyframe_;
  const TimeDelta max_wait_for_frame_;
  video_coding::FrameBuffer frame_buffer_;
  rtc::TaskQueue* const decode_queue_;
  VCMReceiveStatisticsCallback* const stats_proxy_;
  FrameSchedulingReceiver* const receiver_;
  rtc::scoped_refptr<PendingTaskSafetyFlag> decode_safety_ =
      PendingTaskSafetyFlag::CreateDetached();
};

// TODO(eshr): Extract to frame_buffer_constants file.
// Max number of frames the buffer will hold.
constexpr size_t kMaxFramesBuffered = 800;
// Max number of decoded frame info that will be saved.
constexpr int kMaxFramesHistory = 1 << 13;

class FrameBuffer3Proxy : public FrameBufferProxy,
                          public FrameScheduler::Callback {
 public:
  FrameBuffer3Proxy(Clock* clock,
                    TaskQueueBase* worker_queue,
                    VCMTiming* timing,
                    VCMReceiveStatisticsCallback* stats_proxy,
                    rtc::TaskQueue* decode_queue,
                    FrameSchedulingReceiver* receiver,
                    TimeDelta max_wait_for_keyframe,
                    TimeDelta max_wait_for_frame,
                    MetronomeFrameScheduler* metronome_scheduler)
      : max_wait_for_keyframe_(max_wait_for_keyframe),
        max_wait_for_frame_(max_wait_for_frame),
        clock_(clock),
        worker_queue_(worker_queue),
        decode_queue_(decode_queue),
        stats_proxy_(stats_proxy),
        receiver_(receiver),
        timing_(timing),
        jitter_estimator_(clock_),
        inter_frame_delay_(clock_->TimeInMilliseconds()),
        buffer_(kMaxFramesBuffered, kMaxFramesHistory),
        scheduler_(metronome_scheduler) {
    RTC_DCHECK(decode_queue_);
    RTC_DCHECK(stats_proxy_);
    RTC_DCHECK(receiver_);
    RTC_DCHECK(timing_);
    RTC_DCHECK(worker_queue_);
    RTC_DCHECK(clock_);

    if (metronome_scheduler) {
      RTC_LOG(LS_WARNING) << "Using Metronome";
      scheduler_.emplace<MetronomeFrameScheduler*>(metronome_scheduler);
    } else {
      RTC_LOG(LS_WARNING) << "Using FrameBuffer3";
      scheduler_.emplace<FrameScheduler>(
          clock_, worker_queue, timing, &buffer_,
          FrameScheduler::Timeouts{max_wait_for_keyframe_, max_wait_for_frame_},
          this);
    }
  }

  // FrameBufferProxy implementation.
  void StopOnWorker() override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    absl::visit(overloaded{[](FrameScheduler& s) { s.Stop(); },
                           [this](MetronomeFrameScheduler* m) {
                             m->StopSchedulingFrames(&buffer_);
                           }},
                scheduler_);
    started_ = false;
  }

  void SetProtectionMode(VCMVideoProtection protection_mode) override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    protection_mode_ = kProtectionNackFEC;
  }

  void Clear() override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    stats_proxy_->OnDroppedFrames(buffer_.CurrentSize());
    buffer_.Clear();
    if (absl::holds_alternative<FrameScheduler>(scheduler_)) {
      absl::get<FrameScheduler>(scheduler_).OnFrameBufferUpdated();
    }
  }

  absl::optional<int64_t> InsertFrame(
      std::unique_ptr<EncodedFrame> frame) override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    if (frame->is_last_spatial_layer)
      stats_proxy_->OnCompleteFrame(frame->is_keyframe(), frame->size(),
                                    frame->contentType());
    if (!frame->delayed_by_retransmission())
      timing_->IncomingTimestamp(frame->Timestamp(), frame->ReceivedTime());

    buffer_.InsertFrame(std::move(frame));
    if (absl::holds_alternative<FrameScheduler>(scheduler_)) {
      absl::get<FrameScheduler>(scheduler_).OnFrameBufferUpdated();
    }
    return buffer_.LastContinuousFrameId();
  }

  void UpdateRtt(int64_t max_rtt_ms) override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    jitter_estimator_.UpdateRtt(max_rtt_ms);
  }

  void StartNextDecode(bool keyframe_required) override {
    if (!worker_queue_->IsCurrent()) {
      worker_queue_->PostTask(ToQueuedTask(
          worker_safety_,
          [this, keyframe_required] { StartNextDecode(keyframe_required); }));
      return;
    }

    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    if (!started_) {
      started_ = true;
      if (absl::holds_alternative<MetronomeFrameScheduler*>(scheduler_)) {
        absl::get<MetronomeFrameScheduler*>(scheduler_)
            ->StartSchedulingFrames(
                &buffer_,
                FrameScheduler::Timeouts{max_wait_for_keyframe_,
                                         max_wait_for_frame_},
                timing_, this);
      }
    }
    keyframe_required_ = keyframe_required;
    if (keyframe_required) {
      absl::visit(overloaded{[](FrameScheduler& s) { s.ForceKeyFrame(); },
                             [this](MetronomeFrameScheduler* m) {
                               m->ForceKeyFrame(&buffer_);
                             }},
                  scheduler_);
    }
    absl::visit(overloaded{[](FrameScheduler& s) { s.OnReadyForNextFrame(); },
                           [this](MetronomeFrameScheduler* m) {
                             m->OnReceiverReady(&buffer_);
                           }},
                scheduler_);
  }

  int Size() override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    return buffer_.CurrentSize();
  }

  // FrameScheduler::Callbacks implementation
  void OnFrameReady(
      absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames) override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);

    int64_t now_ms = clock_->TimeInMilliseconds();
    RTC_DCHECK(!frames.empty());
    bool superframe_delayed_by_retransmission = false;
    size_t superframe_size = 0;
    const EncodedFrame& first_frame = *frames.front();
    int64_t receive_time_ms = first_frame.ReceivedTime();
    int64_t render_time_ms = first_frame.RenderTimeMs();

    // Gracefully handle bad RTP timestamps and render time issues.
    if (FrameHasBadRenderTiming(render_time_ms, now_ms,
                                timing_->TargetVideoDelay())) {
      jitter_estimator_.Reset();
      timing_->Reset();
      render_time_ms = timing_->RenderTimeMs(first_frame.Timestamp(), now_ms);
    }

    for (std::unique_ptr<EncodedFrame>& frame : frames) {
      frame->SetRenderTime(render_time_ms);

      superframe_delayed_by_retransmission |=
          frame->delayed_by_retransmission();
      receive_time_ms = std::max(receive_time_ms, frame->ReceivedTime());
      superframe_size += frame->size();
    }

    if (!superframe_delayed_by_retransmission) {
      int64_t frame_delay;

      if (inter_frame_delay_.CalculateDelay(first_frame.Timestamp(),
                                            &frame_delay, receive_time_ms)) {
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

    // Update stats.
    UpdateDroppedFrames();
    UpdateJitterDelay();
    UpdateTimingFrameInfo();

    std::unique_ptr<EncodedFrame> frame =
        CombineAndDeleteFrames(std::move(frames));

    // VideoReceiveStream2 wants frames on the decoder thread.
    decode_queue_->PostTask(ToQueuedTask(
        decode_safety_, [this, frame = std::move(frame)]() mutable {
          receiver_->OnEncodedFrame(std::move(frame));
        }));
  }

  void OnTimeout() override {
    RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
    receiver_->OnDecodableFrameTimeout(MaxWait());
  }

 private:
  TimeDelta MaxWait() const RTC_RUN_ON(&worker_sequence_checker_) {
    return keyframe_required_ ? max_wait_for_keyframe_ : max_wait_for_frame_;
  }

  void UpdateDroppedFrames() RTC_RUN_ON(&worker_sequence_checker_) {
    const int dropped_frames = buffer_.GetTotalNumberOfDroppedFrames() -
                               frames_dropped_before_last_new_frame_;
    if (dropped_frames > 0)
      stats_proxy_->OnDroppedFrames(dropped_frames);
    frames_dropped_before_last_new_frame_ =
        buffer_.GetTotalNumberOfDroppedFrames();
  }

  void UpdateJitterDelay() {
    int max_decode_ms;
    int current_delay_ms;
    int target_delay_ms;
    int jitter_buffer_ms;
    int min_playout_delay_ms;
    int render_delay_ms;
    if (timing_->GetTimings(&max_decode_ms, &current_delay_ms, &target_delay_ms,
                            &jitter_buffer_ms, &min_playout_delay_ms,
                            &render_delay_ms)) {
      stats_proxy_->OnFrameBufferTimingsUpdated(
          max_decode_ms, current_delay_ms, target_delay_ms, jitter_buffer_ms,
          min_playout_delay_ms, render_delay_ms);
    }
  }

  void UpdateTimingFrameInfo() {
    absl::optional<TimingFrameInfo> info = timing_->GetTimingFrameInfo();
    if (info)
      stats_proxy_->OnTimingFrameInfoUpdated(*info);
  }

  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_sequence_checker_;
  const TimeDelta max_wait_for_keyframe_;
  const TimeDelta max_wait_for_frame_;
  const absl::optional<RttMultExperiment::Settings> rtt_mult_settings_ =
      RttMultExperiment::GetRttMultValue();
  Clock* const clock_;
  TaskQueueBase* const worker_queue_;
  rtc::TaskQueue* const decode_queue_;
  VCMReceiveStatisticsCallback* const stats_proxy_;
  FrameSchedulingReceiver* const receiver_;
  VCMTiming* const timing_;
  VCMJitterEstimator jitter_estimator_;
  VCMInterFrameDelay inter_frame_delay_;
  bool started_ RTC_GUARDED_BY(&worker_sequence_checker_) = false;
  bool keyframe_required_ RTC_GUARDED_BY(&worker_sequence_checker_) = false;
  FrameBuffer buffer_ RTC_GUARDED_BY(&worker_sequence_checker_);
  absl::variant<FrameScheduler, MetronomeFrameScheduler*> scheduler_
      RTC_GUARDED_BY(&worker_sequence_checker_);
  int frames_dropped_before_last_new_frame_
      RTC_GUARDED_BY(&worker_sequence_checker_) = 0;
  VCMVideoProtection protection_mode_ = kProtectionNack;

  rtc::scoped_refptr<PendingTaskSafetyFlag> decode_safety_ =
      PendingTaskSafetyFlag::CreateDetached();
  ScopedTaskSafety worker_safety_;
};

std::unique_ptr<FrameBufferProxyFactory>
FrameBufferProxyFactory::CreateFromFieldTrial(Clock* clock,
                                              TaskQueueBase* worker_queue) {
  FieldTrialEnum<FrameSchedulerType> type(
      "impl", FrameSchedulerType::kFrameBuffer2,
      {{"FrameBuffer2", FrameSchedulerType::kFrameBuffer2},
       {"FrameBuffer3", FrameSchedulerType::kFrameBuffer3},
       {"Metronome", FrameSchedulerType::kMetronome}});
  ParseFieldTrial({&type}, field_trial::FindFullName("WebRTC-FrameBuffer3"));
  return std::make_unique<FrameBufferProxyFactory>(type.Get(), clock,
                                                   worker_queue);
}

FrameBufferProxyFactory::FrameBufferProxyFactory(FrameSchedulerType arm,
                                                 Clock* clock,
                                                 TaskQueueBase* worker_queue)
    : arm_(arm),
      metronome_scheduler_(
          arm == FrameSchedulerType::kMetronome
              ? std::make_unique<MetronomeFrameScheduler>(clock, worker_queue)
              : nullptr) {}

std::unique_ptr<FrameBufferProxy> FrameBufferProxyFactory::CreateProxy(
    Clock* clock,
    TaskQueueBase* worker_queue,
    VCMTiming* timing,
    VCMReceiveStatisticsCallback* stats_proxy,
    rtc::TaskQueue* decode_queue,
    FrameSchedulingReceiver* receiver,
    TimeDelta max_wait_for_keyframe,
    TimeDelta max_wait_for_frame) {
  switch (arm_) {
    case FrameSchedulerType::kFrameBuffer2:
      return std::make_unique<FrameBuffer2Proxy>(
          clock, timing, stats_proxy, decode_queue, receiver,
          max_wait_for_keyframe, max_wait_for_frame);
    case FrameSchedulerType::kFrameBuffer3:
      return std::make_unique<FrameBuffer3Proxy>(
          clock, worker_queue, timing, stats_proxy, decode_queue, receiver,
          max_wait_for_keyframe, max_wait_for_frame, nullptr);
    case FrameSchedulerType::kMetronome:
      return std::make_unique<FrameBuffer3Proxy>(
          clock, worker_queue, timing, stats_proxy, decode_queue, receiver,
          max_wait_for_keyframe, max_wait_for_frame,
          metronome_scheduler_.get());
  }
  RTC_CHECK_NOTREACHED();
  return nullptr;
}

}  // namespace webrtc
