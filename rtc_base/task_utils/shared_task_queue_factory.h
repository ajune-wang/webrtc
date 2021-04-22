/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_TASK_UTILS_SHARED_TASK_QUEUE_FACTORY_H_
#define RTC_BASE_TASK_UTILS_SHARED_TASK_QUEUE_FACTORY_H_

#include <memory>

#include "api/task_queue/task_queue_factory.h"

namespace webrtc {

// Creates a task queue factory with the property that all task queues created
// through it share a single task queue, created with |base_task_queue_factory|.
// The name and priority will be taken from the first call to CreateTaskQueue.
// The returned factory does not take ownership of |base_task_queue_factory| so
// destroy the returned factory before destroying |base_task_queue_factory|.
std::unique_ptr<TaskQueueFactory> CreateSharedTaskQueueFactory(
    TaskQueueFactory* base_task_queue_factory);

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_SHARED_TASK_QUEUE_FACTORY_H_
