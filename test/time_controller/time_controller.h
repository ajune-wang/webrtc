/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TIME_CONTROLLER_TIME_CONTROLLER_H_
#define TEST_TIME_CONTROLLER_TIME_CONTROLLER_H_

#include <memory>

#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "modules/utility/include/process_thread.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class TimeController {
 public:
  virtual ~TimeController() = default;
  virtual Clock* GetClock() = 0;
  virtual TaskQueueFactory* GetTaskQueueFactory() = 0;
  virtual std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) = 0;
  virtual void Sleep(TimeDelta duration) = 0;
  virtual std::function<void(TaskQueueBase*, QueuedTask*)> TaskInvoker() = 0;
};
}  // namespace webrtc
#endif  // TEST_TIME_CONTROLLER_TIME_CONTROLLER_H_
