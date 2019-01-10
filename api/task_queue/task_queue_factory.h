/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TASK_QUEUE_TASK_QUEUE_FACTORY_H_
#define API_TASK_QUEUE_TASK_QUEUE_FACTORY_H_

#include "api/task_queue/task_queue_base.h"

namespace webrtc {

// Manages creation and destruction of task queues.
class TaskQueueFactory {
 public:
  // TaskQueue priority levels. On some platforms these will map to thread
  // priorities, on others such as Mac and iOS, GCD queue priorities.
  enum class Priority {
    kLow,
    kNormal,
    kHigh,
  };

  virtual ~TaskQueueFactory() = default;
  TaskQueuePtr CreateTaskQueue(const char* name) const {
    return CreateTaskQueue(name, Priority::kNormal);
  }
  virtual TaskQueuePtr CreateTaskQueue(const char* name,
                                       Priority priority) const = 0;
};

}  // namespace webrtc

#endif  // API_TASK_QUEUE_TASK_QUEUE_FACTORY_H_
