/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_TASK_QUEUE_GCD_H_
#define RTC_BASE_TASK_QUEUE_GCD_H_

#include <dispatch/dispatch.h>
#include <memory>

#include "absl/strings/string_view.h"
#include "api/task_queue/task_queue_base.h"

namespace webrtc {

class TaskQueueGcd final : public TaskQueueBase {
 public:
  TaskQueueGcd(absl::string_view queue_name, int gcd_priority);
  void Delete() override;

  void PostTask(std::unique_ptr<QueuedTask> task) override;
  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override;

 private:
  struct TaskContext;
  ~TaskQueueGcd() override;
  static void RunTask(void* task_context);
  static void SetNotActive(void* task_queue);
  static void DeleteContext(void* task_queue);

  dispatch_queue_t queue_;
  bool is_active_;
};

}  // namespace webrtc
#endif  // RTC_BASE_TASK_QUEUE_GCD_H_
