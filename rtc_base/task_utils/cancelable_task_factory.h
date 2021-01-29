/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
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

#include "api/function_view.h"
#include "api/task_queue/queued_task.h"

namespace webrtc {

// Creates QueuedTasks that can be cancelled without destroying the task queues
// they were posted to. Tasks created by the same factory can be posted to
// different task queues.
// When all tasks are posted to and cancelled on the same task queue, it is
// recommended to use cheaper PendingTaskSafetyFlag instead.
// This class is thread safe.
class CancelableTaskFactory {
 public:
  CancelableTaskFactory();
  CancelableTaskFactory(const CancelableTaskFactory&) = delete;
  CancelableTaskFactory& operator=(const CancelableTaskFactory&) = delete;
  ~CancelableTaskFactory();

  // Creates a task that on `Run` invokes `task` or does nothing depending
  // if CancelAll was ever called.
  // Created tasks allowed to outlive this factory.
  template <typename Functor>
  std::unique_ptr<QueuedTask> CreateTask(Functor&& task);

  // Disables running tasks created with `CreateTask`. If there are tasks
  // that are currently runnning, blocks current thread until those task are
  // complete. Tasks that haven't started before CancelAll was called will
  // become noop. It is allowed to create tasks after `CancelAll `is called,
  // but those tasks will be noop.
  // Must be called at least once.
  void CancelAll();

 private:
  class CancelFlagTracker;
  // Helpers to manually manage reference counting of the
  // `CancelFlagTracker` to allow to hide its definition into cc file.
  static void AddRef(CancelFlagTracker& controll);
  static void Release(CancelFlagTracker& controll);

  // Runs the `task` unless `CancelAll` was called.
  static void MaybeRunTask(CancelFlagTracker& flag,
                           rtc::FunctionView<void()> task);

  CancelFlagTracker& flag_;
};

//
// Below are implementaion details.
//
template <typename Functor>
std::unique_ptr<QueuedTask> CancelableTaskFactory::CreateTask(Functor&& task) {
  class Task : public QueuedTask {
   public:
    Task(CancelFlagTracker& flag, Functor&& task)
        : flag_(flag), task_(std::forward<Functor>(task)) {
      CancelableTaskFactory::AddRef(flag_);
    }
    ~Task() override { CancelableTaskFactory::Release(flag_); }

    bool Run() override {
      CancelableTaskFactory::MaybeRunTask(flag_, task_);
      return true;
    }

   private:
    CancelFlagTracker& flag_;
    std::decay_t<Functor> task_;
  };
  return std::make_unique<Task>(flag_, std::forward<Functor>(task));
}

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_CANCELABLE_TASK_FACTORY_H_
