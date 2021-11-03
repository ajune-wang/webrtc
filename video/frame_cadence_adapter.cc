/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_cadence_adapter.h"

#include <memory>
#include <utility>

#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/logging.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

constexpr int64_t FrameCadenceAdapterInterface::kFrameRateAvergingWindowSizeMs;

namespace {

class FrameCadenceAdapterImpl : public FrameCadenceAdapterInterface {
 public:
  explicit FrameCadenceAdapterImpl(Clock* clock);

  // FrameCadenceAdapterInterface overrides.
  void Initialize(Callback* callback) override;
  void SetEnabledByContentType(bool enabled) override;
  absl::optional<uint32_t> GetInputFramerateFps() override;
  void UpdateFrameRate() override;

  // VideoFrameSink overrides.
  void OnFrame(const VideoFrame& frame) override;
  void OnDiscardedFrame() override { callback_->OnDiscardedFrame(); }
  void OnConstraintsChanged(
      const VideoTrackSourceConstraints& constraints) override;

 private:
  Clock* const clock_;
  TaskQueueBase* const main_queue_;

  // Returns true if
  // - Zero-hertz screenshare fieltrial is on
  // - Min FPS set and 0.
  // - Max FPS set and >0.
  // - Content type is enabled.
  bool ZeroHertzModeEnabledLocked() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Called from OnFrame in zero-hertz mode.
  void OnFrameOnMainQueue(VideoFrame frame) RTC_RUN_ON(main_queue_);

  // Called to report on constraint UMAs.
  void MaybeReportFrameRateConstraintUmas()
      RTC_RUN_ON(&incoming_frame_race_checker_) RTC_LOCKS_EXCLUDED(mutex_);

  // True if we support frame entry for screenshare with a minimum frequency of
  // 0 Hz.
  const bool enabled_by_field_trial_;

  // Set up during Initialize.
  Callback* callback_ = nullptr;

  // Lock protecting zero-hertz activation state. This is needed because the
  // threading contexts of OnFrame, OnConstraintsChanged, and ConfigureEncoder
  // are mutating it.
  Mutex mutex_;

  // The source's constraints.
  absl::optional<VideoTrackSourceConstraints> source_constraints_
      RTC_GUARDED_BY(mutex_);

  // Whether operation is enabled by content type (screenshare).
  bool enabled_by_callee_ RTC_GUARDED_BY(mutex_) = false;

  // Race checker for incoming frames. This is the network thread in chromium,
  // but may vary from test contexts.
  rtc::RaceChecker incoming_frame_race_checker_;
  bool has_reported_screenshare_frame_rate_umas_ RTC_GUARDED_BY(mutex_) = false;

  rtc::RaceChecker encoder_sequence_race_checker_
      RTC_GUARDED_BY(encoder_sequence_race_checker_);
  // Input frame rate statistics for use when not in zero-hertz mode.
  RateStatistics input_framerate_
      RTC_GUARDED_BY(encoder_sequence_race_checker_);

  ScopedTaskSafety safety_;
};

FrameCadenceAdapterImpl::FrameCadenceAdapterImpl(Clock* clock)
    : clock_(clock),
      main_queue_(TaskQueueBase::Current()),
      enabled_by_field_trial_(
          field_trial::IsEnabled("WebRTC-ZeroHertzScreenshare")),
      input_framerate_(kFrameRateAvergingWindowSizeMs, 1000) {}

void FrameCadenceAdapterImpl::Initialize(Callback* callback) {
  callback_ = callback;
}

void FrameCadenceAdapterImpl::SetEnabledByContentType(bool enabled) {
  // This method is called on the worker thread.
  MutexLock lock(&mutex_);
  if (enabled && !enabled_by_callee_)
    has_reported_screenshare_frame_rate_umas_ = false;
  enabled_by_callee_ = enabled;
}

absl::optional<uint32_t> FrameCadenceAdapterImpl::GetInputFramerateFps() {
  RTC_DCHECK_RUNS_SERIALIZED(&encoder_sequence_race_checker_);
  MutexLock lock(&mutex_);
  if (ZeroHertzModeEnabledLocked())
    return source_constraints_->max_fps.value();
  return input_framerate_.Rate(clock_->TimeInMilliseconds());
}

void FrameCadenceAdapterImpl::UpdateFrameRate() {
  RTC_DCHECK_RUNS_SERIALIZED(&encoder_sequence_race_checker_);
  input_framerate_.Update(1, clock_->TimeInMilliseconds());
}

void FrameCadenceAdapterImpl::OnFrame(const VideoFrame& frame) {
  // This method is called on the network thread under Chromium, or other
  // various contexts in test.
  RTC_DCHECK_RUNS_SERIALIZED(&incoming_frame_race_checker_);
  main_queue_->PostTask(ToQueuedTask(safety_, [this, frame] {
    RTC_DCHECK_RUN_ON(main_queue_);
    OnFrameOnMainQueue(std::move(frame));
  }));
  MaybeReportFrameRateConstraintUmas();
}

void FrameCadenceAdapterImpl::OnConstraintsChanged(
    const VideoTrackSourceConstraints& constraints) {
  RTC_LOG(LS_INFO) << __func__ << " min_fps "
                   << constraints.min_fps.value_or(-1) << " max_fps "
                   << constraints.max_fps.value_or(-1);
  MutexLock lock(&mutex_);
  source_constraints_ = constraints;
}

bool FrameCadenceAdapterImpl::ZeroHertzModeEnabledLocked() const {
  return enabled_by_field_trial_ && source_constraints_.has_value() &&
         source_constraints_->min_fps.value_or(-1) == 0 &&
         source_constraints_->max_fps.value_or(-1) > 0 && enabled_by_callee_;
}

// RTC_RUN_ON(main_queue_)
void FrameCadenceAdapterImpl::OnFrameOnMainQueue(VideoFrame frame) {
  callback_->OnFrame(frame, absl::nullopt);
}

// RTC_RUN_ON(&incoming_frame_race_checker_)
void FrameCadenceAdapterImpl::MaybeReportFrameRateConstraintUmas() {
  MutexLock lock(&mutex_);
  if (has_reported_screenshare_frame_rate_umas_)
    return;
  has_reported_screenshare_frame_rate_umas_ = true;
  if (!enabled_by_callee_)
    return;
  RTC_HISTOGRAM_BOOLEAN("WebRTC.Screenshare.FrameRateConstraints.Exists",
                        source_constraints_.has_value());
  if (!source_constraints_.has_value())
    return;
  RTC_HISTOGRAM_BOOLEAN("WebRTC.Screenshare.FrameRateConstraints.Min.Exists",
                        source_constraints_->min_fps.has_value());
  if (source_constraints_->min_fps.has_value()) {
    RTC_HISTOGRAM_COUNTS_100(
        "WebRTC.Screenshare.FrameRateConstraints.Min.Value",
        source_constraints_->min_fps.value());
  }
  RTC_HISTOGRAM_BOOLEAN("WebRTC.Screenshare.FrameRateConstraints.Max.Exists",
                        source_constraints_->max_fps.has_value());
  if (source_constraints_->max_fps.has_value()) {
    RTC_HISTOGRAM_COUNTS_100(
        "WebRTC.Screenshare.FrameRateConstraints.Max.Value",
        source_constraints_->max_fps.value());
  }
  if (!source_constraints_->min_fps.has_value()) {
    if (source_constraints_->max_fps.has_value()) {
      RTC_HISTOGRAM_COUNTS_100(
          "WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max",
          source_constraints_->max_fps.value());
    }
  } else if (source_constraints_->max_fps.has_value()) {
    if (source_constraints_->min_fps.value() <
        source_constraints_->max_fps.value()) {
      RTC_HISTOGRAM_COUNTS_100(
          "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min",
          source_constraints_->min_fps.value());
      RTC_HISTOGRAM_COUNTS_100(
          "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max",
          source_constraints_->max_fps.value());
    }
    constexpr int kMaxBucketCount =
        60 * /*max min_fps=*/60 + /*max max_fps=*/60 - 1;
    RTC_HISTOGRAM_ENUMERATION_SPARSE(
        "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne",
        source_constraints_->min_fps.value() * 60 +
            source_constraints_->max_fps.value() - 1,
        /*boundary=*/kMaxBucketCount);
  }
}

}  // namespace

std::unique_ptr<FrameCadenceAdapterInterface>
FrameCadenceAdapterInterface::Create(Clock* clock) {
  return std::make_unique<FrameCadenceAdapterImpl>(clock);
}

}  // namespace webrtc
