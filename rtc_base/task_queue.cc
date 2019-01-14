/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/task_queue.h"

#include "api/task_queue/task_queue_base.h"
#include "rtc_base/task_queue_global_factory.h"

namespace rtc {

TaskQueue::TaskQueue(const char* queue_name, Priority priority)
    : impl_(webrtc::GlobalTaskQueueFactory()
                .CreateTaskQueue(queue_name, priority)
                .release()) {
  impl_->task_queue_ = this;
}

TaskQueue::~TaskQueue() {
  // TODO(danilchap): change impl_ to webrtc::TaskQueuePtr when dependenent
  // projects stop using link-injection to override task queue and thus do not
  // rely on exact TaskQueue layout.
  // There might running task that tries to rescheduler itself to the TaskQueue
  // and not yet away TaskQueue destructor is called.
  // Calling back to TaskQueue::PostTask need impl_ pointer still be valid, so
  // Start the destruction first.
  static_cast<webrtc::TaskQueueBase*>(impl_.get())->Stop();
  // Release the pointer later. Stop is responsible for the deallocation.
  const_cast<scoped_refptr<Impl>&>(impl_).release();
}

// static
TaskQueue* TaskQueue::Current() {
  webrtc::TaskQueueBase* base = webrtc::TaskQueueBase::Current();
  if (base == nullptr) {
    return nullptr;
  }
  return base->task_queue_;
}

bool TaskQueue::IsCurrent() const {
  return Current() == this;
}

void TaskQueue::PostTask(std::unique_ptr<QueuedTask> task) {
  return impl_->PostTask(std::move(task));
}

void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                uint32_t milliseconds) {
  return impl_->PostDelayedTask(std::move(task), milliseconds);
}

}  // namespace rtc
