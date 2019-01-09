/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains the implementation of TaskQueue for Mac and iOS.
// The implementation uses Grand Central Dispatch queues (GCD) to
// do the actual task queuing.

#include "rtc_base/task_queue_gcd.h"

#include <string.h>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

TaskQueueGcd::TaskQueueGcd(const char* queue_name, int gcd_priority)
    : queue_(dispatch_queue_create(queue_name, DISPATCH_QUEUE_SERIAL)),
      context_(new QueueContext(task_queue)) {
  RTC_DCHECK(queue_name);
  RTC_CHECK(queue_);
  dispatch_set_context(queue_, context_);
  // Assign a finalizer that will delete the context when the last reference
  // to the queue is released.  This may run after the TaskQueue object has
  // been deleted.
  dispatch_set_finalizer_f(queue_, &QueueContext::DeleteContext);

  dispatch_set_target_queue(
      queue_, dispatch_get_global_queue(TaskQueuePriorityToGCD(priority), 0));
}

TaskQueueGcd::TaskQueueGcd() {
  RTC_DCHECK(!IsCurrent());
  // Implementation/behavioral note:
  // Dispatch queues are reference counted via calls to dispatch_retain and
  // dispatch_release. Pending blocks submitted to a queue also hold a
  // reference to the queue until they have finished. Once all references to a
  // queue have been released, the queue will be deallocated by the system.
  // This is why we check the context before running tasks.

  // Use dispatch_sync to set the context to null to guarantee that there's not
  // a race between checking the context and using it from a task.
  dispatch_sync_f(queue_, context_, &QueueContext::SetNotActive);
  dispatch_release(queue_);
}

void TaskQueueGcd::PostTask(std::unique_ptr<QueuedTask> task) {
  auto* context = new TaskContext(context_, std::move(task));
  dispatch_async_f(queue_, context, &TaskContext::RunTask);
}

void TaskQueueGcd::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                   uint32_t milliseconds) {
  auto* context = new TaskContext(context_, std::move(task));
  dispatch_after_f(
      dispatch_time(DISPATCH_TIME_NOW, milliseconds * NSEC_PER_MSEC), queue_,
      context, &TaskContext::RunTask);
}

struct TaskQueueGcd::QueueContext {
  explicit QueueContext(TaskQueueGcd* q) : queue(q), is_active(true) {}

  static void SetNotActive(void* context) {
    QueueContext* qc = static_cast<QueueContext*>(context);
    qc->is_active = false;
  }

  static void DeleteContext(void* context) {
    QueueContext* qc = static_cast<QueueContext*>(context);
    delete qc;
  }

  TaskQueueGcd* const queue;
  bool is_active;
};

struct TaskQueueGcd::TaskContext {
  TaskContext(QueueContext* queue_ctx, std::unique_ptr<QueuedTask> task)
      : queue_ctx(queue_ctx), task(std::move(task)) {}
  ~TaskContext() {}

  QueueContext* const queue_ctx;
  std::unique_ptr<QueuedTask> task;
};

void TaskQueueGcd::RunTask(void* task_context) {
  std::unique_ptr<TaskContext> tc(static_cast<TaskContext*>(task_context));
  if (tc->queue_ctx->is_active) {
    CurrentTaskQueueSetter set_current(tc->queue_ctx->queue);
    if (!tc->task->Run())
      tc->task.release();
  }
}

}  // namespace webrtc
