/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/task_queue.h"

#include "api/task_queue/task_queue_base.h"

namespace rtc {
namespace {
struct CurrentTaskQueueSetterExposer : public webrtc::TaskQueueBase {
  using Setter = webrtc::TaskQueueBase::CurrentTaskQueueSetter;
};

}  // namespace

TaskQueue::WrapperTask::WrapperTask(
    std::unique_ptr<QueuedTask> task,
    rtc::scoped_refptr<rtc::FinalRefCountedObject<TaskQueue::SharedState>>
        shared_state)
    : task_(std::move(task)), shared_state_(std::move(shared_state)) {}

bool TaskQueue::WrapperTask::Run() {
  webrtc::MutexLock lock(&shared_state_->task_mu);
  if (!task_->Run())
    (void)task_.release();
  return true;
}

TaskQueue::RefDecrementingWrapperTask::RefDecrementingWrapperTask(
    std::unique_ptr<QueuedTask> task,
    rtc::scoped_refptr<rtc::FinalRefCountedObject<TaskQueue::SharedState>>
        shared_state)
    : WrapperTask(std::move(task), shared_state) {}

TaskQueue::RefDecrementingWrapperTask::~RefDecrementingWrapperTask() {
  webrtc::MutexLock lock(&shared_state_->mu);
  shared_state_->value--;
}

TaskQueue::TaskQueue(
    std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter> task_queue)
    : impl_(task_queue.release()) {}

TaskQueue::~TaskQueue() {
  // There might running task that tries to rescheduler itself to the TaskQueue
  // and not yet aware TaskQueue destructor is called.
  // Calling back to TaskQueue::PostTask need impl_ pointer still be valid, so
  // do not invalidate impl_ pointer until Delete returns.
  impl_->Delete();
}

bool TaskQueue::IsCurrent() const {
  return impl_->IsCurrent();
}

void TaskQueue::PostTask(std::unique_ptr<webrtc::QueuedTask> task) {
  if (!fastpath_enabled_)
    return impl_->PostTask(std::move(task));

  shared_state_->mu.Lock();
  if (shared_state_->value != 0) {
    // Slow path: Queued tasks present. Post normally.
    shared_state_->value++;
    shared_state_->mu.Unlock();
    impl_->PostTask(std::make_unique<RefDecrementingWrapperTask>(
        std::move(task), shared_state_));
    return;
  }

  // Fast path: no queued tasks.
  shared_state_->value = 1;
  shared_state_->task_mu.Lock();
  shared_state_->mu.Unlock();
  {
    CurrentTaskQueueSetterExposer::Setter setter(impl_);
    if (!task->Run())
      (void)task.release();
  }

  shared_state_->mu.Lock();
  shared_state_->task_mu.Unlock();
  // Value might have increased in the unlocked region above.
  shared_state_->value--;
  shared_state_->mu.Unlock();
}

void TaskQueue::PostDelayedTask(std::unique_ptr<webrtc::QueuedTask> task,
                                uint32_t milliseconds) {
  if (!fastpath_enabled_)
    return impl_->PostDelayedTask(std::move(task), milliseconds);
  return impl_->PostDelayedTask(
      std::make_unique<WrapperTask>(std::move(task), shared_state_),
      milliseconds);
}

}  // namespace rtc
