/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_TASK_UTILS_SLACKED_TASK_QUEUE_FACTORY_H_
#define RTC_BASE_TASK_UTILS_SLACKED_TASK_QUEUE_FACTORY_H_

#include <memory>

#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

// Helper class used to trigger delayed calls. The implementation has very weak
// guarantees of when it calls back - it may call back early, or late, but it
// has to be calling delayed in the future.
//
// All methods have to be called on the same sequence the object was created on.
class DelayedCallProvider {
 public:
  virtual ~DelayedCallProvider() = default;

  // Schedule a later call once to |callback| on the sequence used when calling
  // ScheduleDelayedCall. The delay |milliseconds| is given as a hint, but the
  // provider may call earlier or later as it decides based on implementation.
  // If the method has previously been called, the previously scheduled |task|
  // may or may not execute in the future, depending on the implementation. The
  // method has to be called on the same sequence for the lifetime of the
  // object.
  virtual void ScheduleDelayedCall(std::unique_ptr<QueuedTask> task,
                                   uint32_t milliseconds) = 0;
};

// Creates a task queue factory whose task queues have the property that delayed
// tasks execute on timestamps decided by |provider|. The delayed call provider
// may cluster delayed tasks as it wishes, but it will never cause delayed tasks
// to execute before the specified sleep durations. The returned factory does
// not take ownership of |base_task_queue_factory| so destroy the returned
// factory before destroying |base_task_queue_factory|. Note! The returned task
// queue factory does not work with task sources that statically allocates
// QueuedTasks.
//
// The sequence on which this function is called on must match the sequence that
// the delayed call provider was created on.
std::unique_ptr<TaskQueueFactory> CreateSlackedTaskQueueFactory(
    TaskQueueFactory* base_task_queue_factory,
    std::unique_ptr<DelayedCallProvider> provider,
    Clock* clock);

// Creates a delayed call provider that clusters delayed task execution on
// equidistant (|quantum|) instants, from the moment the provider's created. The
// call provider samples the current time at creation in an internal variable.
std::unique_ptr<DelayedCallProvider> CreateQuantumDelayedCallProvider(
    Clock* clock,
    TimeDelta quantum);

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_SLACKED_TASK_QUEUE_FACTORY_H_
