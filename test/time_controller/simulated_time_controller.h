/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_H_
#define TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_H_

#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "api/units/timestamp.h"
#include "modules/include/module.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/thread_checker.h"
#include "test/time_controller/time_controller.h"

namespace webrtc {

namespace sim_time_impl {
class SimulatedSequenceRunner;

class SimulatedTimeControllerImpl : public TaskQueueFactory {
 public:
  explicit SimulatedTimeControllerImpl(Timestamp start_time);
  ~SimulatedTimeControllerImpl() override;

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override;
  std::unique_ptr<ProcessThread> CreateProcessThread(const char* thread_name);

  void RunPending();
  Timestamp CurrentTime() const;
  TimeDelta AdvanceTime(Timestamp limit);

  bool OnCurrentThread() { return thread_id_ == std::this_thread::get_id(); }
  void Unregister(SimulatedSequenceRunner* runner);

 private:
  std::vector<SimulatedSequenceRunner*> GetPending(Timestamp current_time);

  const std::thread::id thread_id_;
  rtc::CriticalSection time_lock_;
  Timestamp current_time_ RTC_GUARDED_BY(time_lock_);
  rtc::CriticalSection lock_;
  std::vector<SimulatedSequenceRunner*> runners_ RTC_GUARDED_BY(lock_);
};
}  // namespace sim_time_impl

// TimeController implementation using completely simulated time. Task queues
// and process threads created by this controller will run delayed activities
// when Sleep() is called.
class SimulatedTimeController : public TimeController {
 public:
  explicit SimulatedTimeController(Timestamp start_time);
  Clock* GetClock() override;
  TaskQueueFactory* GetTaskQueueFactory() override;
  std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) override;
  void Sleep(TimeDelta duration) override;

 private:
  SimulatedClock sim_clock_;
  sim_time_impl::SimulatedTimeControllerImpl impl_;
};

// Similar to SimulatedTimeController, but overrides the global clock backing
// rtc::TimeMillis() and rtc::TimeMicros().
class GlobalSimulatedTimeController : public TimeController {
 public:
  explicit GlobalSimulatedTimeController(Timestamp start_time);
  Clock* GetClock() override;
  TaskQueueFactory* GetTaskQueueFactory() override;
  std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) override;
  void Sleep(TimeDelta duration) override;

 private:
  rtc::ScopedFakeClock global_clock_;
  sim_time_impl::SimulatedTimeControllerImpl impl_;
};
}  // namespace webrtc

#endif  // TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_H_
