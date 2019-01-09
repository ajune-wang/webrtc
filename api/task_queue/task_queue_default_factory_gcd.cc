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

#include "rtc_base/task_queue_gcd.h"

namespace webrtc {
namespace {

int TaskQueuePriorityToGCD(TaskQueueFactory::Priority priority) {
  switch (priority) {
    case TaskQueueFactory::Priority::NORMAL:
      return DISPATCH_QUEUE_PRIORITY_DEFAULT;
    case TaskQueueFactory::Priority::HIGH:
      return DISPATCH_QUEUE_PRIORITY_HIGH;
    case TaskQueueFactory::Priority::LOW:
      return DISPATCH_QUEUE_PRIORITY_LOW;
  }
  RTC_NOTREACHED();
  return DISPATCH_QUEUE_PRIORITY_DEFAULT;
}

}  // namespace

TaskQueuePtr DefaultTaskQueueFactory::CreateTaskQueue(const char* name,
                                                      Priority priority) const {
  return MakeTaskQueuePtr<TaskQueueGcd>(name, TaskQueuePriorityToGCD(priority));
}

}  // namespace webrtc
