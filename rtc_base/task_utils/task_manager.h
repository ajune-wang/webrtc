/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_UTILS_TASK_MANAGER_H_
#define RTC_BASE_TASK_UTILS_TASK_MANAGER_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "api/scoped_refptr.h"
#include "api/task_queue/queued_task.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/event.h"
#include "rtc_base/ref_counter.h"

namespace webrtc {

// Creates QueuedTasks for task queues that can be cancelled without destroying
// the task queues. This class is thread safe.
class StoppableTaskFactory {
 public:
  StoppableTaskFactory();
  StoppableTaskFactory(const StoppableTaskFactory&) = delete;
  StoppableTaskFactory& operator=(const StoppableTaskFactory&) = delete;
  ~StoppableTaskFactory();

  // Create a task that can be posted (possible with a delay) to any task queue.
  // These tasks will not run after Stop returns.
  template <typename Functor>
  std::unique_ptr<QueuedTask> CreateTask(Functor&& task);

  // Disallow running new tasks created with `CreateTask`. If there are
  // currently runnning tasks, blocks current thread until those task are done.
  // It is allowed to create tasks after Stop is called, but those tasks will
  // never be executed.
  void Stop();

 private:
  class ControlBlock;
  const rtc::scoped_refptr<ControlBlock> controll_;
};

//
// Implementaion details are below.
//
class StoppableTaskFactory::ControlBlock {
 public:
  ControlBlock() = default;

  void AddRef();
  void Release();

  void Stop();
  bool StartTask();
  void CompletedTask();

 private:
  ~ControlBlock() = default;

  webrtc_impl::RefCounter ref_count_{0};
  rtc::CriticalSection cs_;
  rtc::Event stopping_;
  bool stopped_ RTC_GUARDED_BY(cs_) = false;
  int running_ RTC_GUARDED_BY(cs_) = 0;
};

template <typename Functor>
std::unique_ptr<QueuedTask> StoppableTaskFactory::CreateTask(Functor&& task) {
  class Task : public QueuedTask {
   public:
    Task(Functor&& task, rtc::scoped_refptr<ControlBlock> controll)
        : functor_(std::forward<Functor>(task)),
          controll_(std::move(controll)) {}

    bool Run() override {
      if (controll_->StartTask()) {
        functor_();
        controll_->CompletedTask();
      }
      return true;
    }

   private:
    std::decay_t<Functor> functor_;
    const rtc::scoped_refptr<ControlBlock> controll_;
  };

  return std::make_unique<Task>(std::forward<Functor>(task), controll_);
}

inline void StoppableTaskFactory::Stop() {
  controll_->Stop();
}

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_TASK_MANAGER_H_
