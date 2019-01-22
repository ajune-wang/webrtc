/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_NETWORK_TIME_CONTROLLER_H_
#define TEST_SCENARIO_NETWORK_TIME_CONTROLLER_H_

#include <memory>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/event.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/task_queue.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

class Activity {
 public:
  virtual ~Activity() = default;

  // Execute activity. |at_time| is time, when activity was executed.
  virtual void Execute(Timestamp at_time) = 0;
  // Returns time, that should pass until next execution of this activity.
  virtual TimeDelta TimeToNextExecution() const = 0;
};

class DelayedActivity : public Activity {
 public:
  // Creates pending activity. |func| should accept time of execution.
  DelayedActivity(std::function<void(Timestamp)> func, TimeDelta delay);
  ~DelayedActivity() override;

  void Execute(Timestamp at_time) override;
  TimeDelta TimeToNextExecution() const override;

 private:
  const std::function<void(Timestamp)> func_;
  const TimeDelta delay_;
  bool executed_;
};

class RepeatedActivity2 : public Activity {
 public:
  RepeatedActivity2(std::function<void(Timestamp)> func, TimeDelta interval);
  RepeatedActivity2(std::function<void(Timestamp)> func,
                    TimeDelta interval,
                    TimeDelta initial_delay);
  ~RepeatedActivity2() override;

  void Execute(Timestamp at_time) override;
  TimeDelta TimeToNextExecution() const override;

 private:
  const std::function<void(Timestamp)> func_;
  const TimeDelta interval_;
  const TimeDelta initial_delay_;

  Timestamp last_executed_at_;
};

// Implementation should be thread safe.
class TimeController {
 public:
  virtual ~TimeController() = default;

  virtual Clock* clock() const = 0;
  // Registers activity to be executed on the controller thread. No any order
  // guaranties are provided between different registered activities.
  virtual void RegisterActivity(std::unique_ptr<Activity> activity) = 0;
  // Will cancel activity if it is still registered in the controller or will do
  // nothing other wise. Returns true if activity was registered and false
  // otherwise.
  virtual bool CancelActivity(Activity* activity) = 0;
  // Starts processing activities
  virtual void Start() = 0;
  // Stops processing activities
  virtual void Stop() = 0;
  // Wait until time controller will be stopped
  virtual void AwaitTermination() = 0;
};

// TaskQueue based TimeController implementation that uses real time clock.
class RealTimeController : public TimeController {
 public:
  RealTimeController();
  ~RealTimeController() override;

  Clock* clock() const override;
  void RegisterActivity(std::unique_ptr<Activity> activity) override;
  bool CancelActivity(Activity* activity) override;
  void Start() override;
  void Stop() override;
  void AwaitTermination() override;

 private:
  enum State { kIdle, kRunning };
  struct ActivityHolder {
    ActivityHolder(std::unique_ptr<Activity> activity,
                   Timestamp next_execution_time);
    ~ActivityHolder();
    std::unique_ptr<Activity> activity;
    Timestamp next_execution_time;
  };

  void ProcessActivities();
  Timestamp Now();

  Clock* const clock_;
  rtc::Event terminated_;

  rtc::CriticalSection lock_;
  State state_ RTC_GUARDED_BY(lock_);
  std::vector<std::unique_ptr<ActivityHolder>> activities_
      RTC_GUARDED_BY(lock_);

  // Must be the last field, so it will be deleted first, because tasks
  // in the TaskQueue can access other fields of the instance of this class.
  rtc::TaskQueue task_queue_;
};

class SimulatedTimeController : public TimeController {
 public:
  explicit SimulatedTimeController(SimulatedClock* clock);
  ~SimulatedTimeController() override;

  // Set global rtc::FakeClock if you need it also be adjusted with simulated
  // clock
  void SetGlobalFakeClock(rtc::FakeClock* global_clock);

  Clock* clock() const override;
  void RegisterActivity(std::unique_ptr<Activity> activity) override;
  bool CancelActivity(Activity* activity) override;
  void Start() override;
  void Stop() override;
  void AwaitTermination() override;

 private:
  enum State { kIdle, kRunning, kTerminating };
  struct ActivityHolder {
    ActivityHolder(std::unique_ptr<Activity> activity,
                   Timestamp next_execution_time);
    ~ActivityHolder();
    std::unique_ptr<Activity> activity;
    Timestamp next_execution_time;
  };

  static void ThreadRun(void* controller);
  void ProcessActivities();
  Timestamp Now();

  SimulatedClock* const clock_;
  rtc::FakeClock* global_clock_;
  rtc::Event start_event_;
  rtc::Event stop_event_;

  rtc::CriticalSection lock_;
  State state_ RTC_GUARDED_BY(lock_);
  std::vector<std::unique_ptr<ActivityHolder>> activities_
      RTC_GUARDED_BY(lock_);

  std::unique_ptr<rtc::PlatformThread> thread_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_NETWORK_TIME_CONTROLLER_H_
