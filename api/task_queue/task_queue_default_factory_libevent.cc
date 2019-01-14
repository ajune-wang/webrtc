/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/task_queue/task_queue_default_factory.h"

#include "rtc_base/checks.h"
#include "rtc_base/task_queue_libevent.h"

namespace webrtc {
namespace {

rtc::ThreadPriority TaskQueuePriorityToThreadPriority(
    TaskQueueFactory::Priority priority) {
  switch (priority) {
    case TaskQueueFactory::Priority::HIGH:
      return rtc::kRealtimePriority;
    case TaskQueueFactory::Priority::LOW:
      return rtc::kLowPriority;
    case TaskQueueFactory::Priority::NORMAL:
      return rtc::kNormalPriority;
    default:
      RTC_NOTREACHED();
      break;
  }
  return rtc::kNormalPriority;
}

}  // namespace

std::unique_ptr<TaskQueueBase, TaskQueueDeleter>
DefaultTaskQueueFactory::CreateTaskQueue(const char* name,
                                         Priority priority) const {
  return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
      new TaskQueueLibevent(name, TaskQueuePriorityToThreadPriority(priority)));
}

}  // namespace webrtc
