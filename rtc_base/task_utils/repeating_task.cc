/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"

namespace webrtc {
namespace webrtc_repeating_task_impl {
RepeatingTaskBase::RepeatingTaskBase(rtc::TaskQueue* task_queue,
                                     TimeDelta first_delay,
                                     IntervalMode interval_mode)
    : task_queue_(task_queue),
      interval_mode_(interval_mode),
      next_run_time_(Timestamp::us(rtc::TimeMicros()) + first_delay) {}

RepeatingTaskBase::~RepeatingTaskBase() = default;

bool RepeatingTaskBase::Run() {
  RTC_DCHECK_RUN_ON(task_queue_);
  // Return true to tell the TaskQueue to destruct this object.
  if (next_run_time_.IsPlusInfinity())
    return true;

  TimeDelta delay = RunClosure();
  RTC_DCHECK(delay.IsFinite());

  TimeDelta lost_time = Timestamp::us(rtc::TimeMicros()) - next_run_time_;
  next_run_time_ += delay;
  if (interval_mode_ == IntervalMode::kIncludingExecution) {
    delay -= lost_time;
  }

  if (delay <= TimeDelta::Zero()) {
    task_queue_->PostTask(absl::WrapUnique(this));
  } else {
    task_queue_->PostDelayedTask(absl::WrapUnique(this), delay.ms());
  }
  // Return false to tell the TaskQueue to not destruct this object since we
  // have taken ownership with absl::WrapUnique.
  return false;
}

void RepeatingTaskBase::Stop() {
  RTC_DCHECK(next_run_time_.IsFinite());
  next_run_time_ = Timestamp::PlusInfinity();
}

void RepeatingTaskBase::PostStop() {
  if (task_queue_->IsCurrent()) {
    RTC_DLOG(LS_INFO) << "Using PostStop() from the task queue running the "
                         "repeated task. Consider calling Stop() instead.";
  }
  task_queue_->PostTask([this] {
    RTC_DCHECK_RUN_ON(task_queue_);
    Stop();
  });
}

}  // namespace webrtc_repeating_task_impl
RepeatingTaskHandle::RepeatingTaskHandle(
    RepeatingTaskHandle::IntervalMode interval_mode)
    : interval_mode_(interval_mode) {
  sequence_checker_.Detach();
}

void RepeatingTaskHandle::Stop() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (repeating_task_) {
    RTC_DCHECK_RUN_ON(repeating_task_->task_queue_);
    repeating_task_->Stop();
    repeating_task_ = nullptr;
  }
}

void RepeatingTaskHandle::PostStop() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_CHECK(repeating_task_);
  repeating_task_->PostStop();
  repeating_task_ = nullptr;
}

bool RepeatingTaskHandle::Running() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return repeating_task_ != nullptr;
}
}  // namespace webrtc
