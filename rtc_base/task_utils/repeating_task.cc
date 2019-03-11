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
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace webrtc_repeating_task_impl {

RepeatingTaskBase::RepeatingTaskBase(std::unique_ptr<SequencedTask> task)
    : task_(std::move(task)) {}

RepeatingTaskBase::~RepeatingTaskBase() = default;

TimeDelta RepeatingTaskBase::Run(Timestamp at_time) {
  if (task_) {
    TimeDelta delay = task_->Run(at_time);
    if (task_) {
      RTC_DCHECK(delay.IsFinite());
      return delay;
    }
  }
  return TimeDelta::PlusInfinity();
}

void RepeatingTaskBase::Stop() {
  task_.reset();
}
}  // namespace webrtc_repeating_task_impl

RepeatingTaskHandle::RepeatingTaskHandle() {
  sequence_checker_.Detach();
}
RepeatingTaskHandle::~RepeatingTaskHandle() {
  sequence_checker_.Detach();
}

RepeatingTaskHandle::RepeatingTaskHandle(RepeatingTaskHandle&& other)
    : repeating_task_(other.repeating_task_) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  other.repeating_task_ = nullptr;
}

RepeatingTaskHandle& RepeatingTaskHandle::operator=(
    RepeatingTaskHandle&& other) {
  RTC_DCHECK_RUN_ON(&other.sequence_checker_);
  {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    repeating_task_ = other.repeating_task_;
  }
  other.repeating_task_ = nullptr;
  return *this;
}

RepeatingTaskHandle::RepeatingTaskHandle(RepeatingTaskBase* repeating_task)
    : repeating_task_(repeating_task) {}

void RepeatingTaskHandle::Stop() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (repeating_task_) {
    repeating_task_->Stop();
    repeating_task_ = nullptr;
  }
}

bool RepeatingTaskHandle::Running() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return repeating_task_ != nullptr;
}

}  // namespace webrtc
