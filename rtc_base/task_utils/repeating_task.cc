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

#include <memory>

#include "absl/functional/any_invocable.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

struct RepeatingTask {
  RepeatingTask(TaskQueueBase* task_queue,
                TaskQueueBase::DelayPrecision precision,
                TimeDelta first_delay,
                absl::AnyInvocable<TimeDelta()> task,
                Clock* clock,
                rtc::scoped_refptr<PendingTaskSafetyFlag> alive_flag)
      : task_queue(task_queue),
        precision(precision),
        clock(clock),
        task(std::move(task)),
        next_run_time(clock->CurrentTime() + first_delay),
        alive_flag(std::move(alive_flag)) {}

  TaskQueueBase* const task_queue;
  const TaskQueueBase::DelayPrecision precision;
  Clock* const clock;
  absl::AnyInvocable<TimeDelta()> task;
  // This is always finite.
  Timestamp next_run_time RTC_GUARDED_BY(task_queue);
  rtc::scoped_refptr<PendingTaskSafetyFlag> alive_flag
      RTC_GUARDED_BY(task_queue);
};

absl::AnyInvocable<void() &&> RunLater(std::unique_ptr<RepeatingTask> task);

void RunNow(std::unique_ptr<RepeatingTask> repeating_task) {
  RTC_DCHECK_RUN_ON(repeating_task->task_queue);
  if (!repeating_task->alive_flag->alive())
    return;

  webrtc_repeating_task_impl::RepeatingTaskImplDTraceProbeRun();
  TimeDelta delay = repeating_task->task();
  RTC_DCHECK_GE(delay, TimeDelta::Zero());

  // A delay of +infinity means that the task should not be run again.
  // Alternatively, the closure might have stopped this task.
  if (delay.IsPlusInfinity() || !repeating_task->alive_flag->alive())
    return;

  TimeDelta lost_time =
      repeating_task->clock->CurrentTime() - repeating_task->next_run_time;
  repeating_task->next_run_time += delay;
  delay -= lost_time;
  delay = std::max(delay, TimeDelta::Zero());

  TaskQueueBase* task_queue = repeating_task->task_queue;
  TaskQueueBase::DelayPrecision precision = repeating_task->precision;
  task_queue->PostDelayedTaskWithPrecision(
      precision, RunLater(std::move(repeating_task)), delay);
}

absl::AnyInvocable<void() &&> RunLater(std::unique_ptr<RepeatingTask> task) {
  return [task = std::move(task)]() mutable { RunNow(std::move(task)); };
}

}  // namespace

RepeatingTaskHandle RepeatingTaskHandle::Start(
    TaskQueueBase* task_queue,
    absl::AnyInvocable<TimeDelta()> closure,
    TaskQueueBase::DelayPrecision precision,
    Clock* clock) {
  auto alive_flag = PendingTaskSafetyFlag::CreateDetached();
  webrtc_repeating_task_impl::RepeatingTaskHandleDTraceProbeStart();
  task_queue->PostTask(RunLater(
      std::make_unique<RepeatingTask>(task_queue, precision, TimeDelta::Zero(),
                                      std::move(closure), clock, alive_flag)));
  return RepeatingTaskHandle(std::move(alive_flag));
}

// DelayedStart is equivalent to Start except that the first invocation of the
// closure will be delayed by the given amount.
RepeatingTaskHandle RepeatingTaskHandle::DelayedStart(
    TaskQueueBase* task_queue,
    TimeDelta first_delay,
    absl::AnyInvocable<TimeDelta()> closure,
    TaskQueueBase::DelayPrecision precision,
    Clock* clock) {
  auto alive_flag = PendingTaskSafetyFlag::CreateDetached();
  webrtc_repeating_task_impl::RepeatingTaskHandleDTraceProbeDelayedStart();
  task_queue->PostDelayedTaskWithPrecision(
      precision,
      RunLater(std::make_unique<RepeatingTask>(task_queue, precision,
                                               first_delay, std::move(closure),
                                               clock, alive_flag)),
      first_delay);
  return RepeatingTaskHandle(std::move(alive_flag));
}

void RepeatingTaskHandle::Stop() {
  if (repeating_task_) {
    repeating_task_->SetNotAlive();
    repeating_task_ = nullptr;
  }
}

bool RepeatingTaskHandle::Running() const {
  return repeating_task_ != nullptr;
}

namespace webrtc_repeating_task_impl {
// These methods are empty, but can be externally equipped with actions using
// dtrace.
void RepeatingTaskHandleDTraceProbeStart() {}
void RepeatingTaskHandleDTraceProbeDelayedStart() {}
void RepeatingTaskImplDTraceProbeRun() {}
}  // namespace webrtc_repeating_task_impl
}  // namespace webrtc
