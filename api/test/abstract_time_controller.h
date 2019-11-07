/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_ABSTRACT_TIME_CONTROLLER_H_
#define API_TEST_ABSTRACT_TIME_CONTROLLER_H_

#include <functional>
#include <memory>

#include "absl/strings/string_view.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/utility/include/process_thread.h"
#include "system_wrappers/include/clock.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

// Base class for TimeController implementations.
//
// The base class takes care of scheduling and execution of tasks, and
// overriding the global rtc clock.  Subclasses must provide a clock and a
// mechanism to schedule calls back to AbstractTimeController at a given time
// (according to that clock).
class AbstractTimeController : public TimeController, public TaskQueueFactory {
 public:
  explicit AbstractTimeController(Clock* clock);

  // Implementation of TimeController.
  Clock* GetClock() override;
  TaskQueueFactory* GetTaskQueueFactory() override;
  std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) override;
  void Sleep(TimeDelta duration) override;
  void InvokeWithControlledYield(std::function<void()> closure) override;

  // Implementation of TaskQueueFactory.
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      TaskQueueFactory::Priority priority) const override;

 protected:
  // Executes any tasks scheduled at or before the current time.  May call
  // |ScheduleAt| to schedule the next call to |Run|.
  void Run();

  // Schedules a call to |Run| at |time|.  |ScheduleAt| may be called multiple
  // times before |Run|.  When this occurs, |Run| should be called once, at the
  // minimum of all scheduled times.
  virtual void ScheduleAt(Timestamp time) = 0;

  // Advances time by |duration|.  Invokes any scheduled calls to |Run|.
  virtual void RunFor(TimeDelta duration) = 0;

 private:
  class ProcessThreadWrapper;
  class TaskQueueWrapper;

  void UpdateTime();
  void ScheduleNext();

  Clock* clock_;
  sim_time_impl::SimulatedTimeControllerImpl impl_;
  rtc::ScopedBaseFakeClock global_clock_;
};

}  // namespace webrtc

#endif  // API_TEST_ABSTRACT_TIME_CONTROLLER_H_
