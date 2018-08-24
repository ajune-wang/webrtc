/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CANCELABLE_TASK_HANDLER_H_
#define RTC_BASE_CANCELABLE_TASK_HANDLER_H_

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/task_queue.h"

namespace rtc {

// This class is thread safe.
class CancelableTaskHandler {
 public:
  // This class is copyable and cheaply movable.
  CancelableTaskHandler();
  CancelableTaskHandler(const CancelableTaskHandler&);
  CancelableTaskHandler(CancelableTaskHandler&&);
  CancelableTaskHandler& operator=(const CancelableTaskHandler&);
  CancelableTaskHandler& operator=(CancelableTaskHandler&&);
  // Deleteing the handler doesn't Cancel the task.
  ~CancelableTaskHandler();

  // Starts a new |task| on |task_queue| in |initial_delay_ms| or immidiately if
  // delay is zero.
  // task() should return time in ms until next run or zero if task shouldn't
  // run again.
  // Starting new task doesn't cancel old one.
  template <typename Closure>
  void StartPeriodicTask(Closure&& task,
                         rtc::TaskQueue* task_queue,
                         int initial_delay_ms);

  // Prevents scheduling new runnings of task() started by StartPeriodicTask.
  // Doesn't wait if task() is already running.
  void Cancel();

 private:
  class CancelationToken;
  class BaseTask : public QueuedTask {
   public:
    BaseTask();
    ~BaseTask() override;
    // Postpone construction after templated Task is constructed to avoid
    // exposing CancellationToken details into header file.
    void Construct(rtc::TaskQueue* task_queue,
                   const rtc::scoped_refptr<CancelationToken>& token);

   protected:
    bool Canceled();
    void Reschedule(int delay_ms) {
      task_queue_->PostDelayedTask(absl::WrapUnique(this), delay_ms);
    }

   private:
    rtc::TaskQueue* task_queue_;
    rtc::scoped_refptr<CancelationToken> cancelation_token_;
  };

  template <typename Closure>
  class Task : public CancelableTaskHandler::BaseTask {
   public:
    explicit Task(Closure&& closure)
        : closure_(std::forward<Closure>(closure)) {}
    ~Task() override = default;

   private:
    bool Run() override {
      if (BaseTask::Canceled())
        return true;
      int delay_ms = closure_();
      if (delay_ms <= 0)
        return true;
      BaseTask::Reschedule(delay_ms);
      return false;
    }

    Closure closure_;
  };

  void StartPeriodicTaskInternal(std::unique_ptr<BaseTask> task,
                                 rtc::TaskQueue* task_queue,
                                 int initial_delay_ms);

  rtc::scoped_refptr<CancelationToken> cancelation_token_;
};

template <typename Closure>
void CancelableTaskHandler::StartPeriodicTask(Closure&& closure,
                                              rtc::TaskQueue* task_queue,
                                              int initial_delay_ms) {
  auto task = absl::make_unique<Task<Closure>>(std::forward<Closure>(closure));
  StartPeriodicTaskInternal(std::move(task), task_queue, initial_delay_ms);
}

}  // namespace rtc

#endif  // RTC_BASE_CANCELABLE_TASK_HANDLER_H_
