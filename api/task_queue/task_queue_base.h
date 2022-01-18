/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TASK_QUEUE_TASK_QUEUE_BASE_H_
#define API_TASK_QUEUE_TASK_QUEUE_BASE_H_

#include <memory>
#include <utility>

#include "api/task_queue/queued_task.h"
#include "api/units/time_delta.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// Asynchronously executes tasks in a way that guarantees that they're executed
// in FIFO order and that tasks never overlap. Tasks may always execute on the
// same worker thread and they may not. To DCHECK that tasks are executing on a
// known task queue, use IsCurrent().
class RTC_LOCKABLE RTC_EXPORT TaskQueueBase {
 public:
  // Starts destruction of the task queue.
  // On return ensures no task are running and no new tasks are able to start
  // on the task queue.
  // Responsible for deallocation. Deallocation may happen synchronously during
  // Delete or asynchronously after Delete returns.
  // Code not running on the TaskQueue should not make any assumption when
  // TaskQueue is deallocated and thus should not call any methods after Delete.
  // Code running on the TaskQueue should not call Delete, but can assume
  // TaskQueue still exists and may call other methods, e.g. PostTask.
  // Should be called on the same task queue or thread that this task queue
  // was created on.
  virtual void Delete() = 0;

  // Schedules a task to execute. Tasks are executed in FIFO order.
  // If `task->Run()` returns true, task is deleted on the task queue
  // before next QueuedTask starts executing.
  // When a TaskQueue is deleted, pending tasks will not be executed but they
  // will be deleted. The deletion of tasks may happen synchronously on the
  // TaskQueue or it may happen asynchronously after TaskQueue is deleted.
  // This may vary from one implementation to the next so assumptions about
  // lifetimes of pending tasks should not be made.
  // May be called on any thread or task queue, including this task queue.
  virtual void PostTask(std::unique_ptr<QueuedTask> task) = 0;

  // Prefer PostDelayedTask() over PostDelayedTaskWithHighPrecision() whenever
  // possible.
  //
  // Schedules a task to execute a specified number of milliseconds from when
  // the call is made, using "low" precision. Unlike "high" precision which is
  // "best effort", the TaskQueueBase implementation is allowed to schedule with
  // leeway of up to 17 ms later than what the caller specified. This leeway is
  // in addition to OS timer limitations which typically have a precision of
  // ~1 ms, but there are notable exceptions when the OS timer can be off by as
  // much as 15 ms (e.g. Windows on battery).
  //
  // The leeway allows coalescing multiple delayed tasks to the same wake up
  // time, increasing the likelihood that the CPU can become idle in-between
  // tasks. This is the preferred method for scheduling delayed tasks, but
  // high precision use cases can schedule tasks without this leeway using
  // PostDelayedTaskWithHighPrecision() instead.
  //
  // If low precision is not supported by the implementation, PostDelayedTask()
  // has the same precision as PostDelayedTaskWithHighPrecision(). See Chromium
  // implementation status (https://crbug.com/1253787). WebRTC implementations
  // currently do not support low precision.
  //
  // May be called on any thread or task queue, including this task queue.
  virtual void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                               uint32_t milliseconds) = 0;

  // Prefer PostDelayedTask() over PostDelayedTaskWithHighPrecision() whenever
  // possible.
  //
  // Schedules a task to execute a specified number of milliseconds from when
  // the call is made, using "high" precision. The precision should still be
  // considered as "best effort" and in some cases, such as on Windows when all
  // high precision timers have been used up (e.g. when running on battery),
  // can be off by as much as 15 millseconds. In most cases though, the OS timer
  // precision is ~1 ms.
  //
  // Using PostDelayedTaskWithHighPrecision() will cause the CPU to wake up if
  // it is idle. Doing this frequently prevents CPUs from reaching lower power
  // consuming C-states and even infrequent scheduling can add up when there are
  // multiple sources. This is why PostDelayedTask() is preferred in most cases.
  //
  // May be called on any thread or task queue, including this task queue.
  virtual void PostDelayedTaskWithHighPrecision(
      std::unique_ptr<QueuedTask> task,
      TimeDelta delay) {
    // Remove default implementation when dependencies have implemented this
    // method.
    PostDelayedTask(std::move(task), static_cast<uint32_t>(delay.ms()));
  }

  // Returns the task queue that is running the current thread.
  // Returns nullptr if this thread is not associated with any task queue.
  // May be called on any thread or task queue, including this task queue.
  static TaskQueueBase* Current();
  bool IsCurrent() const { return Current() == this; }

 protected:
  class CurrentTaskQueueSetter {
   public:
    explicit CurrentTaskQueueSetter(TaskQueueBase* task_queue);
    CurrentTaskQueueSetter(const CurrentTaskQueueSetter&) = delete;
    CurrentTaskQueueSetter& operator=(const CurrentTaskQueueSetter&) = delete;
    ~CurrentTaskQueueSetter();

   private:
    TaskQueueBase* const previous_;
  };

  // Users of the TaskQueue should call Delete instead of directly deleting
  // this object.
  virtual ~TaskQueueBase() = default;
};

struct TaskQueueDeleter {
  void operator()(TaskQueueBase* task_queue) const { task_queue->Delete(); }
};

}  // namespace webrtc

#endif  // API_TASK_QUEUE_TASK_QUEUE_BASE_H_
