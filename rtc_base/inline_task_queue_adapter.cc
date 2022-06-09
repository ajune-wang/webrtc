/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/inline_task_queue_adapter.h"

#include <memory>
#include <utility>

#include "api/scoped_refptr.h"
#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {

InlineTaskQueueAdapter::InlineTaskQueueAdapter(
    std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base_task_queue)
    : base_task_queue_(std::move(base_task_queue)) {}

void InlineTaskQueueAdapter::Delete() {
  base_task_queue_ = nullptr;
}

void InlineTaskQueueAdapter::PostTask(std::unique_ptr<QueuedTask> task) {
  if (shared_state_->TryBeginInlineExecution()) {
    CurrentTaskQueueSetter setter(this);
    if (!task->Run())
      (void)task.release();
    shared_state_->EndExecution();
  } else {
    // A queue slot was already taken for us in TryBeginInlineExecution.
    base_task_queue_->PostTask(std::make_unique<WrapperTask>(
        std::move(task), shared_state_, this, /*take_queue_slot=*/false));
  }
}

void InlineTaskQueueAdapter::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                             uint32_t millis) {
  base_task_queue_->PostDelayedTask(
      std::make_unique<WrapperTask>(std::move(task), shared_state_, this,
                                    /*take_queue_slot=*/true),
      millis);
}

void InlineTaskQueueAdapter::PostDelayedHighPrecisionTask(
    std::unique_ptr<QueuedTask> task,
    uint32_t millis) {
  base_task_queue_->PostDelayedHighPrecisionTask(
      std::make_unique<WrapperTask>(std::move(task), shared_state_, this,
                                    /*take_queue_slot=*/true),
      millis);
}

bool InlineTaskQueueAdapter::SharedState::TryBeginInlineExecution() {
  MutexLock lock(&mu_);
  if (queue_size_++ == 0) {
    // Need to lock `task_mu` here atomically with the `queue_size_`increasing.
    // Otherwise, deferred or delayed tasks might come in and execute
    // concurrently with us doing the inline execution.
    task_mu_.Lock();
    return true;
  }
  return false;
}

void InlineTaskQueueAdapter::SharedState::EndExecution() {
  task_mu_.Unlock();
  MutexLock lock(&mu_);
  RTC_DCHECK(queue_size_ >= 1);
  queue_size_--;
}

void InlineTaskQueueAdapter::SharedState::BeginExecution(bool take_queue_slot)
    RTC_EXCLUSIVE_LOCK_FUNCTION() {
  MutexLock lock(&mu_);
  if (take_queue_slot)
    queue_size_++;
  task_mu_.Lock();
}

InlineTaskQueueAdapter::WrapperTask::WrapperTask(
    std::unique_ptr<QueuedTask> task,
    rtc::scoped_refptr<rtc::FinalRefCountedObject<SharedState>> shared,
    TaskQueueBase* queue,
    bool take_queue_slot)
    : task_(std::move(task)),
      shared_state_(std::move(shared)),
      queue_(queue),
      take_queue_slot_(take_queue_slot) {}

bool InlineTaskQueueAdapter::WrapperTask::Run() {
  CurrentTaskQueueSetter setter(queue_);
  shared_state_->BeginExecution(take_queue_slot_);
  if (!task_->Run())
    (void)task_.release();
  shared_state_->EndExecution();
  return true;
}

}  // namespace webrtc
