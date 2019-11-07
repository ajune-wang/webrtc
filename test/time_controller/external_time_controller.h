/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TIME_CONTROLLER_EXTERNAL_TIME_CONTROLLER_H_
#define TEST_TIME_CONTROLLER_EXTERNAL_TIME_CONTROLLER_H_

#include <memory>

#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/timestamp.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/synchronization/yield_policy.h"
#include "test/time_controller/simulated_time_controller.h"
#include "test/time_controller/time_controller.h"

namespace webrtc {

// Generic interface for an external time controller.
class ExternalController {
 public:
  virtual ~ExternalController() = default;

  virtual Clock* GetClock() = 0;
  virtual void ScheduleAt(Timestamp time) = 0;
  virtual void RunFor(TimeDelta duration) = 0;
};

// Simulated-time controller that works in lockstep with an external controller.
class ExternalTimeController : public TimeController, public TaskQueueFactory {
 public:
  explicit ExternalTimeController(ExternalController* const controller);

  rtc::YieldInterface* YieldInterface();
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      TaskQueueFactory::Priority priority) const override;

  // TimeController implementation.
  Clock* GetClock() override;
  TaskQueueFactory* GetTaskQueueFactory() override;
  std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) override;
  void Sleep(TimeDelta duration) override;
  void InvokeWithControlledYield(std::function<void()> closure) override;

  void ScheduleNext();
  void Run();

 private:
  ExternalController* const controller_;
  sim_time_impl::SimulatedTimeControllerImpl impl_;
};

}  // namespace webrtc

#endif  // TEST_TIME_CONTROLLER_EXTERNAL_TIME_CONTROLLER_H_
