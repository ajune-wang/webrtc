/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_TASK_UTILS_QUANTUM_TASK_QUEUE_FACTORY_H_
#define RTC_BASE_TASK_UTILS_QUANTUM_TASK_QUEUE_FACTORY_H_

#include <memory>

#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

// Creates a task queue factory with the property that all delayed tasks execute
// coalesced on time instants decided by the specified |quantum_delay|.
// The returned factory does not take ownership of |base_task_queue_factory| so
// destroy the returned factory before destroying |base_task_queue_factory|.
// Note! The returned task queue factory does not work with task sources that
// statically allocates QueuedTasks.
std::unique_ptr<TaskQueueFactory> CreateQuantumTaskQueueFactory(
    TaskQueueFactory* base_task_queue_factory,
    TimeDelta quantum_delay,
    Clock* clock);

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_QUANTUM_TASK_QUEUE_FACTORY_H_
