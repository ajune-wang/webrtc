/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TASK_QUEUE_TASK_QUEUE_IMPL_H_
#define API_TASK_QUEUE_TASK_QUEUE_IMPL_H_

#include <memory>

#include "api/task_queue/queued_task.h"
#include "rtc_base/task_queue.h"

// TODO(danilchap): Remove Impl and dependency on rtc::TaskQueue when custom
// implementations switch to use global factories that creates TaskQueue
// instead of using link-time injection.
class rtc::TaskQueue::Impl {
 public:
  virtual void PostTask(std::unique_ptr<QueuedTask> task) = 0;

  // Schedules a task to execute a specified number of milliseconds from when
  // the call is made. The precision should be considered as "best effort"
  // and in some cases, such as on Windows when all high precision timers have
  // been used up, can be off by as much as 15 millseconds (although 8 would be
  // more likely). This can be mitigated by limiting the use of delayed tasks.
  virtual void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                               uint32_t milliseconds) = 0;

  void AddRef();
  void Release();

 protected:
  virtual ~Impl() = default;

 private:
  friend class rtc::TaskQueue;
  rtc::TaskQueue* task_queue_ = nullptr;
};

#endif  // API_TASK_QUEUE_TASK_QUEUE_IMPL_H_
