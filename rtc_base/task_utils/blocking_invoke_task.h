/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_TASK_UTILS_BLOCKING_INVOKE_TASK_H_
#define RTC_BASE_TASK_UTILS_BLOCKING_INVOKE_TASK_H_

#include <memory>
#include <utility>

#include "api/task_queue/task_queue_base.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {

// Posts a task and waits for it to finish before continuing. This can incur a
// large runtime cost and if it is called from another task on a thread pool
// it can cause a deadlock. Be careful when using this and ask for advice if
// you think you need to add this in a new place but feel uncertain about the
// implications. The posted task must return true.
void BlockingInvokeTask(TaskQueueBase* task_queue,
                        std::unique_ptr<QueuedTask> task);

template <class Closure,
          typename std::enable_if<!std::is_convertible<
              Closure,
              std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
void BlockingInvokeTask(TaskQueueBase* task_queue, Closure&& closure) {
  BlockingInvokeTask(task_queue, ToQueuedTask(std::forward<Closure>(closure)));
}
}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_BLOCKING_INVOKE_TASK_H_
