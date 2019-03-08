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

#include "api/task_queue/global_task_queue_factory.h"
#include "api/task_queue/task_queue_base.h"
namespace webrtc {
namespace task_queue_impl {
void StoppableTaskWrapper::Stop() {
  task_.reset();
}

StoppableTaskWrapper::StoppableTaskWrapper(std::unique_ptr<SequencedTask> task)
    : task_(std::move(task)) {}

TimeDelta StoppableTaskWrapper::Run(Timestamp at_time) {
  if (task_) {
    TimeDelta delay = task_->Run(at_time);
    if (task_) {
      RTC_DCHECK(delay.IsFinite());
      return delay;
    }
  }
  return TimeDelta::PlusInfinity();
}

}  // namespace task_queue_impl

RepeatingTaskHandle::RepeatingTaskHandle(
    task_queue_impl::StoppableTaskWrapper* handle)
    : handle_(handle) {}

RepeatingTaskHandle::RepeatingTaskHandle() {}

RepeatingTaskHandle::~RepeatingTaskHandle() {}

RepeatingTaskHandle::RepeatingTaskHandle(RepeatingTaskHandle&& other)
    : handle_(other.handle_) {
  other.handle_ = nullptr;
}

RepeatingTaskHandle& RepeatingTaskHandle::operator=(
    RepeatingTaskHandle&& other) {
  handle_ = other.handle_;
  other.handle_ = nullptr;
  return *this;
}

void RepeatingTaskHandle::Stop() {
  if (handle_) {
    handle_->Stop();
    handle_ = nullptr;
  }
}

bool RepeatingTaskHandle::Running() const {
  return handle_ != nullptr;
}
}  // namespace webrtc

namespace rtc {

TaskQueue::TaskQueue(
    std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter> task_queue)
    : impl_(task_queue.release()) {}

TaskQueue::TaskQueue(const char* queue_name, Priority priority)
    : TaskQueue(webrtc::GlobalTaskQueueFactory().CreateTaskQueue(queue_name,
                                                                 priority)) {}

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

void TaskQueue::PostTask(std::unique_ptr<QueuedTask> task) {
  return impl_->PostTask(std::move(task));
}

void TaskQueue::BlockingInvokeTask(std::unique_ptr<webrtc::QueuedTask> task) {
  impl_->BlockingInvokeTask(std::move(task));
}

void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                uint32_t milliseconds) {
  return impl_->PostDelayedTask(std::move(task), milliseconds);
}

}  // namespace rtc
