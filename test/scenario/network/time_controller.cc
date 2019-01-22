/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/network/time_controller.h"

#include <utility>

#include "absl/memory/memory.h"

namespace webrtc {
namespace test {
namespace {

constexpr int64_t kDefaultProcessingIntervalMs = 1;

}  // namespace

DelayedActivity::DelayedActivity(std::function<void(Timestamp)> func,
                                 TimeDelta delay)
    : func_(func), delay_(delay), executed_(false) {}
DelayedActivity::~DelayedActivity() = default;

void DelayedActivity::Execute(Timestamp at_time) {
  RTC_CHECK(!executed_);
  func_(at_time);
  executed_ = true;
}

TimeDelta DelayedActivity::TimeToNextExecution() const {
  if (executed_) {
    return TimeDelta::PlusInfinity();
  }
  return delay_;
}

RepeatedActivity2::RepeatedActivity2(std::function<void(Timestamp)> func,
                                     TimeDelta interval)
    : RepeatedActivity2(func, interval, TimeDelta::us(0)) {}
RepeatedActivity2::RepeatedActivity2(std::function<void(Timestamp)> func,
                                     TimeDelta interval,
                                     TimeDelta initial_delay)
    : func_(func),
      interval_(interval),
      initial_delay_(initial_delay),
      last_executed_at_(Timestamp::MinusInfinity()) {}
RepeatedActivity2::~RepeatedActivity2() = default;

void RepeatedActivity2::Execute(Timestamp at_time) {
  last_executed_at_ = at_time;
  func_(at_time);
}

TimeDelta RepeatedActivity2::TimeToNextExecution() const {
  if (last_executed_at_.IsMinusInfinity()) {
    return initial_delay_;
  }
  return interval_;
}

RealTimeController::RealTimeController()
    : clock_(webrtc::Clock::GetRealTimeClock()),
      state_(State::kIdle),
      task_queue_("real_time_controller") {}
RealTimeController::~RealTimeController() = default;

Clock* RealTimeController::clock() const {
  return clock_;
}

void RealTimeController::RegisterActivity(std::unique_ptr<Activity> activity) {
  rtc::CritScope crit(&lock_);
  TimeDelta time_to_next_execution = activity->TimeToNextExecution();
  if (time_to_next_execution.IsPlusInfinity()) {
    // This activity needn't to be executed.
    return;
  }
  activities_.push_back(absl::make_unique<ActivityHolder>(
      std::move(activity), Now() + time_to_next_execution));
}

bool RealTimeController::CancelActivity(Activity* activity) {
  rtc::CritScope crit(&lock_);
  for (auto it = activities_.begin(); it != activities_.end(); ++it) {
    ActivityHolder* holder = it->get();
    if (holder->activity.get() == activity) {
      activities_.erase(it);
      return true;
    }
  }
  return false;
}

void RealTimeController::Start() {
  rtc::CritScope crit(&lock_);
  RTC_CHECK(state_ == State::kIdle);
  terminated_.Reset();
  state_ = State::kRunning;
  ProcessActivities();
}

void RealTimeController::Stop() {
  rtc::CritScope crit(&lock_);
  if (state_ == State::kIdle) {
    return;
  }
  state_ = State::kIdle;
}

void RealTimeController::AwaitTermination() {
  terminated_.Wait(rtc::Event::kForever);
}

void RealTimeController::ProcessActivities() {
  Timestamp current_time = Timestamp::us(0);
  Timestamp next_execution_time = Timestamp::us(0);
  do {
    rtc::CritScope crit(&lock_);
    // Stop processing if requested.
    if (state_ != State::kRunning) {
      terminated_.Set();
      return;
    }

    // Refresh time values. It can happen, that we wait on lock too long and
    // previous value is outdated.
    current_time = Now();
    // Set |next_execution_time| to plus infinity to then find a closest
    // required once.
    next_execution_time = Timestamp::PlusInfinity();
    for (auto it = activities_.begin(); it != activities_.end();) {
      ActivityHolder* holder = it->get();
      if (holder->next_execution_time <= current_time) {
        holder->activity->Execute(current_time);
        TimeDelta delay = holder->activity->TimeToNextExecution();
        if (delay.IsPlusInfinity()) {
          activities_.erase(it);
          continue;
        }
        holder->next_execution_time = current_time + delay;
        if (holder->next_execution_time < next_execution_time) {
          next_execution_time = holder->next_execution_time;
        }
      }
      ++it;
    }
    // Activities processing can take a lot of time, so we need to refresh
    // |current_time| value.
    current_time = Now();
    // We are continuing while we needn't wait until next execution.
    // Compare ms here, because task queue will require ms resolution for wait
    // time, so ms difference should be positive.
  } while (next_execution_time.IsFinite() &&
           (next_execution_time - current_time).ms() <= 0);

  int64_t delay_ms = kDefaultProcessingIntervalMs;
  if (!next_execution_time.IsPlusInfinity()) {
    delay_ms = (next_execution_time - current_time).ms();
  }
  RTC_CHECK_GT(delay_ms, 0);
  task_queue_.PostDelayedTask([this]() { ProcessActivities(); }, delay_ms);
}

Timestamp RealTimeController::Now() {
  return Timestamp::us(clock_->TimeInMicroseconds());
}

RealTimeController::ActivityHolder::ActivityHolder(
    std::unique_ptr<Activity> activity,
    Timestamp next_execution_time)
    : activity(std::move(activity)),
      next_execution_time(std::move(next_execution_time)) {}
RealTimeController::ActivityHolder::~ActivityHolder() = default;

SimulatedTimeController::SimulatedTimeController(SimulatedClock* clock)
    : clock_(clock), global_clock_(nullptr), state_(State::kIdle) {
  thread_ = absl::make_unique<rtc::PlatformThread>(
      &SimulatedTimeController::ThreadRun, this, "simulated_time_controller");
  thread_->Start();
}
SimulatedTimeController::~SimulatedTimeController() {
  {
    rtc::CritScope crit(&lock_);
    state_ = State::kTerminating;
    start_event_.Set();
  }
  thread_->Stop();
}

void SimulatedTimeController::SetGlobalFakeClock(rtc::FakeClock* global_clock) {
  global_clock_ = global_clock;
}

Clock* SimulatedTimeController::clock() const {
  return clock_;
}

void SimulatedTimeController::RegisterActivity(
    std::unique_ptr<Activity> activity) {
  rtc::CritScope crit(&lock_);
  TimeDelta time_to_next_execution = activity->TimeToNextExecution();
  if (time_to_next_execution.IsPlusInfinity()) {
    // This activity needn't to be executed.
    return;
  }
  activities_.push_back(absl::make_unique<ActivityHolder>(
      std::move(activity), Now() + time_to_next_execution));
}

bool SimulatedTimeController::CancelActivity(Activity* activity) {
  rtc::CritScope crit(&lock_);
  for (auto it = activities_.begin(); it != activities_.end(); ++it) {
    ActivityHolder* holder = it->get();
    if (holder->activity.get() == activity) {
      activities_.erase(it);
      return true;
    }
  }
  return false;
}

void SimulatedTimeController::Start() {
  rtc::CritScope crit(&lock_);
  RTC_CHECK(state_ == State::kIdle);
  state_ = State::kRunning;
  stop_event_.Reset();
  start_event_.Set();
}

void SimulatedTimeController::Stop() {
  rtc::CritScope crit(&lock_);
  if (state_ != State::kRunning) {
    return;
  }
  state_ = State::kIdle;
}

void SimulatedTimeController::AwaitTermination() {
  {
    rtc::CritScope crit(&lock_);
    if (state_ != State::kRunning) {
      return;
    }
  }
  stop_event_.Wait(rtc::Event::kForever);
}

void SimulatedTimeController::ThreadRun(void* controller) {
  SimulatedTimeController* time_controller =
      static_cast<SimulatedTimeController*>(controller);
  time_controller->ProcessActivities();
}

void SimulatedTimeController::ProcessActivities() {
  while (true) {
    rtc::CritScope crit(&lock_);
    // If |state_| was changed to kRunning before this thread waits on
    // |start_event_|, then event will be turned on. Then when |state_| will be
    // changed to kIdle, this thread will just go through event, turning it off.
    // So we need to wait while |state_| is still kIdle, until someone will
    // change the |state_| and refire |start_event_|.
    while (state_ == State::kIdle) {
      stop_event_.Set();
      lock_.Leave();
      start_event_.Wait(rtc::Event::kForever);
      start_event_.Reset();
      lock_.Enter();
    }
    if (state_ == State::kTerminating) {
      return;
    }

    // Refresh time values. It can happen, that we wait on lock too long and
    // previous value is outdated.
    Timestamp current_time = Now();
    // Set |next_execution_time| to plus infinity to then find a closest
    // required once.
    Timestamp next_execution_time = Timestamp::PlusInfinity();
    for (auto it = activities_.begin(); it != activities_.end();) {
      ActivityHolder* holder = it->get();
      if (holder->next_execution_time <= current_time) {
        holder->activity->Execute(current_time);
        TimeDelta delay = holder->activity->TimeToNextExecution();
        if (delay.IsPlusInfinity()) {
          activities_.erase(it);
          continue;
        }
        holder->next_execution_time = current_time + delay;
      }
      if (holder->next_execution_time < next_execution_time) {
        next_execution_time = holder->next_execution_time;
      }
      ++it;
    }
    // Activities processing can take a lot of time, so we need to refresh
    // |current_time| value.
    current_time = Now();

    TimeDelta delay_ms = TimeDelta::ms(1);
    if (!next_execution_time.IsPlusInfinity()) {
      delay_ms = next_execution_time - current_time;
    }
    RTC_CHECK_GT(delay_ms.ms(), 0);
    clock_->AdvanceTimeMicroseconds(delay_ms.us());
    if (global_clock_) {
      global_clock_->SetTimeNanos(clock_->TimeInMicroseconds() * 1000);
    }
  }
}

Timestamp SimulatedTimeController::Now() {
  return Timestamp::us(clock_->TimeInMicroseconds());
}

SimulatedTimeController::ActivityHolder::ActivityHolder(
    std::unique_ptr<Activity> activity,
    Timestamp next_execution_time)
    : activity(std::move(activity)),
      next_execution_time(std::move(next_execution_time)) {}
SimulatedTimeController::ActivityHolder::~ActivityHolder() = default;

}  // namespace test
}  // namespace webrtc
