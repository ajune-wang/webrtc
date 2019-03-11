/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_UTILS_REPEATING_TASK_H_
#define RTC_BASE_TASK_UTILS_REPEATING_TASK_H_

#include <type_traits>
#include <utility>

#include "absl/memory/memory.h"
#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

namespace webrtc_repeating_task_impl {

// Represents a task that can be stopped.
class RepeatingTaskBase : public SequencedTask {
 public:
  explicit RepeatingTaskBase(std::unique_ptr<SequencedTask> task);
  // Destroyed by the owning task queue after being stopped or on destruction
  // of the task queue.
  virtual ~RepeatingTaskBase();

  // Implements SequencedTask
  TimeDelta Run(Timestamp at_time) override;

  // Stops the task, called at most once. Must be called on the task queue
  // that owns the task that is being stopped.
  void Stop();

 private:
  std::unique_ptr<SequencedTask> task_;
};

// Helper class to wrap lambda closures.
template <class Closure>
class RepeatingClosureWrapper;

template <class Closure>
class RepeatingClosureWrapper<TimeDelta (Closure::*)(Timestamp)>
    : public SequencedTask {
 public:
  explicit RepeatingClosureWrapper(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  TimeDelta Run(Timestamp at_time) override { return closure_(at_time); }
  typename std::decay<Closure>::type closure_;
};

template <class Closure>
class RepeatingClosureWrapper<TimeDelta (Closure::*)()> : public SequencedTask {
 public:
  explicit RepeatingClosureWrapper(Closure&& closure)
      : closure_(std::forward<Closure>(closure)) {}
  TimeDelta Run(Timestamp at_time) override { return closure_(); }
  typename std::decay<Closure>::type closure_;
};

template <class>
struct StripConst;
template <class R, class C, class... Args>
struct StripConst<R (C::*)(Args...)> {
  using type = R (C::*)(Args...);
};
template <class R, class C, class... Args>
struct StripConst<R (C::*)(Args...) const> {
  using type = R (C::*)(Args...);
};

template <class Closure, class CallSignature = decltype(&Closure::operator())>
std::unique_ptr<SequencedTask> WrapRepeatingClosure(Closure&& closure) {
  return absl::make_unique<
      RepeatingClosureWrapper<typename StripConst<CallSignature>::type>>(
      std::forward<Closure>(closure));
}

}  // namespace webrtc_repeating_task_impl

// Represents a repeating task that can be stopped. When it has been assigned a
// task it is in the running stage. It's always ok to call Stop, but it will not
// do anything in the non-running state. All members must be called from the
// task queue the target is running on.
class RepeatingTaskHandle {
 public:
  RepeatingTaskHandle();
  RepeatingTaskHandle(RepeatingTaskHandle&& other);
  RepeatingTaskHandle(const RepeatingTaskHandle&) = delete;
  RepeatingTaskHandle& operator=(RepeatingTaskHandle&& other);
  RepeatingTaskHandle& operator=(const RepeatingTaskHandle&) = delete;
  ~RepeatingTaskHandle();

  // Stops future invocations of the repeating task closure. Can only be called
  // from the TaskQueue where the task is running. The closure is guaranteed to
  // not be running after Stop() returns unless Stop() is called from the
  // closure itself.
  void Stop();

  // Indicates that this task is running and has not been stopped.
  bool Running() const;

  // Posts a task to repeat |closure| on the provided task queue.
  // The task will be repeated with a delay indicated by the TimeDelta return
  // value of |closure|. |closure| can optionally receive a Timestamp indicating
  // the time when it's run. The actual task is owned by the TaskQueue and will
  // live until it has been stopped or the TaskQueue is destroyed. Note that
  // this means that trying to stop the repeating task after the TaskQueue is
  // destroyed is an error. However, it's perfectly fine to destroy the handle
  // while the task is running, since the repeated task is owned by the
  // TaskQueue.
  template <class Closure>
  static RepeatingTaskHandle Start(TaskQueueBase* task_queue,
                                   Closure&& closure) {
    return CreateRepeatHandle(task_queue, TimeDelta::Zero(),
                              webrtc_repeating_task_impl::WrapRepeatingClosure(
                                  std::forward<Closure>(closure)));
  }

  // DelayedStart is equivalent to Start except that the first invocation will
  // be delayed by |first_delay|.
  template <class Closure>
  static RepeatingTaskHandle DelayedStart(TaskQueueBase* task_queue,
                                          TimeDelta first_delay,
                                          Closure&& closure) {
    return CreateRepeatHandle(task_queue, first_delay,
                              webrtc_repeating_task_impl::WrapRepeatingClosure(
                                  std::forward<Closure>(closure)));
  }


 private:
  using RepeatingTaskBase = webrtc_repeating_task_impl::RepeatingTaskBase;
  explicit RepeatingTaskHandle(RepeatingTaskBase* handle);
  static RepeatingTaskHandle CreateRepeatHandle(
      TaskQueueBase* task_queue,
      TimeDelta first_delay,
      std::unique_ptr<SequencedTask> task) {
    auto stoppable = absl::make_unique<RepeatingTaskBase>(std::move(task));
    auto* stoppable_ptr = stoppable.get();
    task_queue->PostRepeatingTask(first_delay, std::move(stoppable));
    return RepeatingTaskHandle(stoppable_ptr);
  }
  rtc::SequencedTaskChecker sequence_checker_;
  // Owned by the task queue.
  RepeatingTaskBase* repeating_task_ = nullptr;
};
}  // namespace webrtc
#endif  // RTC_BASE_TASK_UTILS_REPEATING_TASK_H_
