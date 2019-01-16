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
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class RepeatingTaskHandle;

namespace webrtc_repeating_task_impl {
enum class IntervalMode {
  // Interpret the returned delay as inclusive of execution time. This means
  // that extra delay and execution time is compensated for. This is appropriate
  // for timed tasks where it's important to keep a specified update rate.
  kIncludingExecution,
  // Interpret the returned delay as exclusive of execution time. This is
  // appropriate for resource intensive tasks without strict timing
  // requirements.
  kExcludingExecution
};
class RepeatingTaskBase : public rtc::QueuedTask {
 public:
  RepeatingTaskBase(rtc::TaskQueue* task_queue,
                    TimeDelta first_delay,
                    IntervalMode interval_mode);
  ~RepeatingTaskBase() override;
  virtual TimeDelta RunClosure() = 0;

 private:
  friend class ::webrtc::RepeatingTaskHandle;

  bool Run() final;
  void Stop() RTC_RUN_ON(task_queue_);
  void PostStop();

  rtc::TaskQueue* const task_queue_;
  const IntervalMode interval_mode_;
  // This is always finite, except for the special case where it's PlusInfinity
  // to signal that the task should stop.
  Timestamp next_run_time_ RTC_GUARDED_BY(task_queue_);
};

// The template closure pattern is based on rtc::ClosureTask.
template <class Closure>
class RepeatingTaskImpl final : public RepeatingTaskBase {
 public:
  RepeatingTaskImpl(rtc::TaskQueue* task_queue,
                    TimeDelta first_delay,
                    IntervalMode interval_mode,
                    Closure&& closure)
      : RepeatingTaskBase(task_queue, first_delay, interval_mode),
        closure_(std::forward<Closure>(closure)) {
    static_assert(
        std::is_same<TimeDelta,
                     typename std::result_of<decltype (&Closure::operator())(
                         Closure)>::type>::value,
        "");
  }

  TimeDelta RunClosure() override { return closure_(); }

 private:
  typename std::remove_const<
      typename std::remove_reference<Closure>::type>::type closure_;
};
}  // namespace webrtc_repeating_task_impl

// Allows starting tasks that repeat themselves on a TaskQueue indefinately
// until they are stopped or the TaskQueue is destroyed. It allows starting and
// stopping multiple times, but you must stop one task before starting another
// and it can only be stopped when in the running state. The public interface is
// not thread safe.
class RepeatingTaskHandle {
 public:
  using IntervalMode = webrtc_repeating_task_impl::IntervalMode;
  // |interval_mode| specifies how the returned interval should be interpreted.
  explicit RepeatingTaskHandle(
      IntervalMode interval_mode = IntervalMode::kIncludingExecution);
  RTC_DISALLOW_COPY_AND_ASSIGN(RepeatingTaskHandle);

  // Start can be used to start a task that will be reposted with a delay
  // determined by the return value of the provided closure. The actual task is
  // owned by the TaskQueue and will live until it has been stopped or the
  // TaskQueue is destroyed. Note that this means that trying to stop the
  // repeating task after the TaskQueue is destroyed is an error. However, it's
  // perfectly fine to destroy the handle while the task is running, since the
  // repeated task is owned by the TaskQueue.
  template <class Closure>
  void Start(rtc::TaskQueue* task_queue, Closure&& closure) {
    RTC_DCHECK(!repeating_task_);
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    auto repeating_task = absl::make_unique<
        webrtc_repeating_task_impl::RepeatingTaskImpl<Closure>>(
        task_queue, TimeDelta::Zero(), interval_mode_,
        std::forward<Closure>(closure));
    repeating_task_ = repeating_task.get();
    task_queue->PostTask(std::move(repeating_task));
  }
  template <class Closure>
  void Start(Closure&& closure) {
    Start(rtc::TaskQueue::Current(), std::forward<Closure>(closure));
  }

  // DelayStart is equivalent to Start except that the first invocation of the
  // closure will be delayed by the given amount.
  template <class Closure>
  void DelayStart(rtc::TaskQueue* task_queue,
                  TimeDelta first_delay,
                  Closure&& closure) {
    RTC_DCHECK(!repeating_task_);
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    auto repeating_task = absl::make_unique<
        webrtc_repeating_task_impl::RepeatingTaskImpl<Closure>>(
        task_queue, first_delay, interval_mode_,
        std::forward<Closure>(closure));
    repeating_task_ = repeating_task.get();
    task_queue->PostDelayedTask(std::move(repeating_task), first_delay.ms());
  }
  template <class Closure>
  void DelayStart(TimeDelta first_delay, Closure&& closure) {
    DelayStart(rtc::TaskQueue::Current(), first_delay,
               std::forward<Closure>(closure));
  }

  // Stops future invocations of the repeating task closure. Can only be called
  // from the TaskQueue where the task is running. The closure is guaranteed to
  // not be running after Stop() returns unless Stop() is called from the
  // closure itself.
  void Stop();

  // Stops future invocations of the repeating task closure. The closure might
  // still be running when PostStop() returns, but there will be no future
  // invocation.
  void PostStop();

  // Returns true if Start() or DelayStart() was called most recently. Returns
  // false initially and if Stop() or PostStop() was called most recently.
  bool Running() const;

 private:
  const IntervalMode interval_mode_;
  rtc::SequencedTaskChecker sequence_checker_;
  // Owned by the task queue.
  webrtc_repeating_task_impl::RepeatingTaskBase* repeating_task_ = nullptr;
};

}  // namespace webrtc
#endif  // RTC_BASE_TASK_UTILS_REPEATING_TASK_H_
