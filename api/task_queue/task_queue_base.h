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

#include "api/task_queue/queued_task.h"

namespace webrtc {

// Asynchronously executes tasks in a way that guarantees that they're executed
// in FIFO order and that tasks never overlap. Tasks may always execute on the
// same worker thread and they may not. To DCHECK that tasks are executing on a
// known task queue, use IsCurrent().
class TaskQueueBase {
 public:
  // Starts destruction of the task queue. Responsible for deallocation.
  // Implementation may complete deallocation on a different thread after Delete
  // is returned. Once Delete is called, PostTask and PostDelayTask allowed to
  // be called only on the TaskQueue.
  // When a TaskQueue is deleted, pending tasks will not be executed but they
  // will be deleted.  The deletion of tasks may happen asynchronously after the
  // TaskQueue itself has been deleted or it may happen synchronously while the
  // TaskQueue is deleted. This may vary from one implementation to the next so
  // assumptions about lifetimes of pending tasks should not be made.
  virtual void Delete() = 0;

  // Schedules a task to execute.
  virtual void PostTask(std::unique_ptr<QueuedTask> task) = 0;

  // Schedules a task to execute a specified number of milliseconds from when
  // the call is made. The precision should be considered as "best effort"
  // and in some cases, such as on Windows when all high precision timers have
  // been used up, can be off by as much as 15 millseconds.
  virtual void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                               uint32_t milliseconds) = 0;

  // Until all TaskQueue implementations switch to  using CurrentTaskQueueSetter
  // below, this function may return nullptr even if code is executed by a
  // TaskQueue. Keep using rtc::TaskQueue::Current() until bugs.webrtc.org/10191
  // is resolved.
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
