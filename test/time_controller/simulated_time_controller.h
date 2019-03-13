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
}  // namespace sim_time_impl

class SimulatedTimeController : public TimeController,
                                private TaskQueueFactory {
 public:
  explicit SimulatedTimeController(Timestamp start_time,
                                   bool override_global_clock = false);
  ~SimulatedTimeController() override;
  Clock* GetClock() override;
  TaskQueueFactory* GetTaskQueueFactory() override;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override;
  std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) override;

  void Wait(TimeDelta duration) override;

 private:
  friend class sim_time_impl::SimulatedSequenceRunner;
  using Runner = sim_time_impl::SimulatedSequenceRunner;

  TaskQueueBase* CreateTaskQueue(absl::string_view name);
  void Unregister(Runner* runner);
  Runner* AdvanceTimeAndGetNextRunner(Timestamp next_time);
  bool OnCurrentThread() { return thread_id_ == std::this_thread::get_id(); }

  const std::thread::id thread_id_;
  rtc::CriticalSection lock_;
  Timestamp current_time_ RTC_GUARDED_BY(lock_);
  std::vector<Runner*> runners_ RTC_GUARDED_BY(lock_);
  SimulatedClock sim_clock_;
  std::unique_ptr<rtc::ScopedFakeClock> rtc_fake_clock_;
};
}  // namespace webrtc

#endif  // TEST_TIME_CONTROLLER_SIMULATED_TIME_CONTROLLER_H_
