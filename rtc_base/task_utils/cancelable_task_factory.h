/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_UTILS_CANCELABLE_TASK_FACTORY_H_
#define RTC_BASE_TASK_UTILS_CANCELABLE_TASK_FACTORY_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "api/scoped_refptr.h"
#include "api/task_queue/queued_task.h"
#include "rtc_base/event.h"
#include "rtc_base/ref_counter.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

// Creates QueuedTasks that can be cancelled without destroying the task queues
// they were posted to. Tasks created by the same factory can be posted to
// different task queues.
// When all tasks are posted to are cancelled on the same task queue, it is
// recommended to use cheaper PendingTaskSafetyFlag.
// This class is thread safe.
class CancelableTaskFactory {
 public:
  CancelableTaskFactory();
  CancelableTaskFactory(const CancelableTaskFactory&) = delete;
  CancelableTaskFactory& operator=(const CancelableTaskFactory&) = delete;
  ~CancelableTaskFactory() = default;

  // Create a task that can be posted (possible with a delay) to any task queue.
  // These tasks will not run after CancelAll returns.
  template <typename Functor>
  std::unique_ptr<QueuedTask> CreateTask(Functor&& task);

  // Disallow running new tasks created with `CreateTask`. If there are
  // currently runnning tasks, blocks current thread until those task are done.
  // It is allowed to create tasks after Stop is called, but those tasks will
  // never be executed.
  void CancelAll() { controll_->CancelAll(); }

 private:
  class ControlBlock {
   public:
    ControlBlock() = default;

    void AddRef();
    void Release();

    void CancelAll();
    bool StartTask();
    void MarkTaskCompleted();

   private:
    ~ControlBlock() = default;

    webrtc_impl::RefCounter ref_count_{0};
    Mutex mu_;
    rtc::Event canceling_{true, false};
    bool canceled_ RTC_GUARDED_BY(mu_) = false;
    // number of tasks created by this factory that are currently running.
    // Because tasks might be running on different task queues, there might be
    // more than one.
    int num_running_ RTC_GUARDED_BY(mu_) = 0;
  };

  const rtc::scoped_refptr<ControlBlock> controll_;
};

//
// Implementaion details are below.
//
template <typename Functor>
std::unique_ptr<QueuedTask> CancelableTaskFactory::CreateTask(Functor&& task) {
  class Task : public QueuedTask {
   public:
    Task(Functor&& task, rtc::scoped_refptr<ControlBlock> controll)
        : functor_(std::forward<Functor>(task)),
          controll_(std::move(controll)) {}

    bool Run() override {
      if (controll_->StartTask()) {
        functor_();
        controll_->MarkTaskCompleted();
      }
      return true;
    }

   private:
    std::decay_t<Functor> functor_;
    const rtc::scoped_refptr<ControlBlock> controll_;
  };

  return std::make_unique<Task>(std::forward<Functor>(task), controll_);
}

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_CANCELABLE_TASK_FACTORY_H_
