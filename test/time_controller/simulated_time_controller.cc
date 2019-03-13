/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/time_controller/simulated_time_controller.h"

#include <algorithm>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"

namespace webrtc {
namespace sim_time_impl {
class SimulatedSequenceRunner : public ProcessThread, public TaskQueueBase {
 public:
  SimulatedSequenceRunner(SimulatedTimeController* handler,
                          absl::string_view queue_name)
      : handler_(handler), clock_(handler_->GetClock()), name_(queue_name) {}
  ~SimulatedSequenceRunner() override { handler_->Unregister(this); }

  Timestamp GetNextRunTime() const;

  void Run();
  void TimeUpdate();

  // TaskQueueBase interface
  void Delete() override;
  // Note: PostTask is also in ProcessThread interface.
  void PostTask(std::unique_ptr<QueuedTask> task) override;
  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override;

  // ProcessThread interface
  void Start() override;
  void Stop() override;
  void WakeUp(Module* module) override;
  void RegisterModule(Module* module, const rtc::Location& from) override;
  void DeRegisterModule(Module* module) override;

 private:
  Timestamp GetCurrentTime() const {
    return Timestamp::us(clock_->TimeInMicroseconds());
  }
  void RunPendingTasks(Timestamp at_time) RTC_LOCKS_EXCLUDED(lock_);
  void RunPendingModules(Timestamp at_time) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void UpdateProcessTime() RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  SimulatedTimeController* const handler_;
  Clock* const clock_;
  const std::string name_;

  rtc::CriticalSection lock_;

  std::deque<std::unique_ptr<QueuedTask>> pending_tasks_ RTC_GUARDED_BY(lock_);
  std::multimap<Timestamp, std::unique_ptr<QueuedTask>> delayed_tasks_
      RTC_GUARDED_BY(lock_);

  bool process_thread_running_ RTC_GUARDED_BY(lock_) = false;
  std::set<Module*> stopped_modules_ RTC_GUARDED_BY(lock_);
  std::set<Module*> pending_modules_ RTC_GUARDED_BY(lock_);
  std::multimap<Timestamp, Module*> delayed_modules_ RTC_GUARDED_BY(lock_);

  Timestamp next_run_time_ RTC_GUARDED_BY(lock_) = Timestamp::PlusInfinity();
};

Timestamp SimulatedSequenceRunner::GetNextRunTime() const {
  rtc::CritScope lock(&lock_);
  return next_run_time_;
}

void SimulatedSequenceRunner::Run() {
  Timestamp at_time = GetCurrentTime();
  RunPendingTasks(at_time);
  rtc::CritScope lock(&lock_);
  RunPendingModules(at_time);
  UpdateProcessTime();
}

void SimulatedSequenceRunner::TimeUpdate() {
  Timestamp at_time = GetCurrentTime();
  rtc::CritScope lock(&lock_);
  for (auto it = delayed_tasks_.begin();
       it != delayed_tasks_.end() && it->first <= at_time;) {
    pending_tasks_.emplace_back(std::move(it->second));
    it = delayed_tasks_.erase(it);
  }
  for (auto it = delayed_modules_.begin();
       it != delayed_modules_.end() && it->first <= at_time;) {
    pending_modules_.insert(it->second);
    it = delayed_modules_.erase(it);
  }
}

void SimulatedSequenceRunner::Delete() {
  rtc::CritScope lock(&lock_);
  pending_tasks_.clear();
  delayed_tasks_.clear();
}

void SimulatedSequenceRunner::RunPendingTasks(Timestamp at_time) {
  decltype(pending_tasks_) pending_tasks;
  {
    rtc::CritScope lock(&lock_);
    pending_tasks.swap(pending_tasks_);
  }
  if (!pending_tasks.empty()) {
    CurrentTaskQueueSetter set_current(this);
    for (auto& pending : pending_tasks) {
      bool delete_task = pending->Run();
      if (delete_task) {
        pending.reset();
      } else {
        pending.release();
      }
    }
  }
}

void SimulatedSequenceRunner::RunPendingModules(Timestamp at_time) {
  if (!pending_modules_.empty()) {
    CurrentTaskQueueSetter set_current(this);
    for (auto* module : pending_modules_) {
      module->Process();
      Timestamp next_run_time =
          at_time + TimeDelta::ms(module->TimeUntilNextProcess());
      delayed_modules_.insert({next_run_time, module});
    }
  }
  pending_modules_.clear();
}

void SimulatedSequenceRunner::UpdateProcessTime() {
  if (!pending_tasks_.empty() || !pending_modules_.empty()) {
    next_run_time_ = Timestamp::MinusInfinity();
  } else {
    next_run_time_ = Timestamp::PlusInfinity();
    if (!delayed_tasks_.empty())
      next_run_time_ = std::min(next_run_time_, delayed_tasks_.begin()->first);
    if (!delayed_modules_.empty())
      next_run_time_ =
          std::min(next_run_time_, delayed_modules_.begin()->first);
  }
}

void SimulatedSequenceRunner::PostTask(std::unique_ptr<QueuedTask> task) {
  if (!IsCurrent() && handler_->OnCurrentThread()) {
    // Running the pending task up to the posting tasks in case it is a blocking
    // task that requires to be run before the calling code can continue. Note
    // that this breaks synchronisation guarantees if the task posts a task back
    // to the orginating task queue.
    CurrentTaskQueueSetter set_current(this);
    RunPendingTasks(GetCurrentTime());
    if (!task->Run())
      task.release();
    rtc::CritScope lock(&lock_);
    UpdateProcessTime();
  } else {
    rtc::CritScope lock(&lock_);
    pending_tasks_.emplace_back(std::move(task));
    next_run_time_ = Timestamp::MinusInfinity();
  }
}

void SimulatedSequenceRunner::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                              uint32_t milliseconds) {
  rtc::CritScope lock(&lock_);
  Timestamp target_time = GetCurrentTime() + TimeDelta::ms(milliseconds);
  delayed_tasks_.insert({target_time, std::move(task)});
  next_run_time_ = std::min(next_run_time_, target_time);
}

void SimulatedSequenceRunner::Start() {
  std::set<Module*> starting;
  {
    rtc::CritScope lock(&lock_);
    if (process_thread_running_)
      return;
    process_thread_running_ = true;
    starting.swap(stopped_modules_);
  }
  for (auto& module : starting)
    module->ProcessThreadAttached(this);

  Timestamp at_time = GetCurrentTime();
  rtc::CritScope lock(&lock_);
  for (auto& module : starting)
    delayed_modules_.insert(
        {at_time + TimeDelta::ms(module->TimeUntilNextProcess()), module});
  UpdateProcessTime();
}

void SimulatedSequenceRunner::Stop() {
  std::set<Module*> stopping;
  {
    rtc::CritScope lock(&lock_);
    process_thread_running_ = false;

    for (auto* pending : pending_modules_)
      stopped_modules_.insert(pending);
    pending_modules_.clear();

    for (auto& delayed : delayed_modules_)
      stopped_modules_.insert(delayed.second);
    delayed_modules_.clear();

    stopping = stopped_modules_;
  }
  for (auto& module : stopping)
    module->ProcessThreadAttached(nullptr);
}

void SimulatedSequenceRunner::WakeUp(Module* module) {
  rtc::CritScope lock(&lock_);
  // If we already are planning to run this module as soon as possible, we don't
  // need to do anything.
  if (pending_modules_.find(module) != pending_modules_.end())
    return;

  for (auto it = delayed_modules_.begin(); it != delayed_modules_.end(); ++it) {
    if (it->second == module) {
      delayed_modules_.erase(it);
      break;
    }
  }
  Timestamp next_time =
      GetCurrentTime() + TimeDelta::ms(module->TimeUntilNextProcess());
  delayed_modules_.insert({next_time, module});
  next_run_time_ = std::min(next_run_time_, next_time);
}

void SimulatedSequenceRunner::RegisterModule(Module* module,
                                             const rtc::Location& from) {
  module->ProcessThreadAttached(this);
  rtc::CritScope lock(&lock_);
  if (!process_thread_running_) {
    stopped_modules_.insert(module);
  } else {
    Timestamp next_time =
        GetCurrentTime() + TimeDelta::ms(module->TimeUntilNextProcess());
    delayed_modules_.insert({next_time, module});
    next_run_time_ = std::min(next_run_time_, next_time);
  }
}

void SimulatedSequenceRunner::DeRegisterModule(Module* module) {
  bool modules_running;
  {
    rtc::CritScope lock(&lock_);
    if (!process_thread_running_) {
      stopped_modules_.erase(module);
    } else {
      pending_modules_.erase(module);
      for (auto it = delayed_modules_.begin(); it != delayed_modules_.end();
           ++it) {
        if (it->second == module) {
          delayed_modules_.erase(it);
          break;
        }
      }
    }
    modules_running = process_thread_running_;
  }
  if (modules_running)
    module->ProcessThreadAttached(nullptr);
}

}  // namespace sim_time_impl

SimulatedTimeController::SimulatedTimeController(Timestamp start_time,
                                                 bool override_global_clock)
    : thread_id_(std::this_thread::get_id()),
      current_time_(start_time),
      sim_clock_(current_time_.us()) {
  if (override_global_clock) {
    rtc_fake_clock_ = absl::make_unique<rtc::ScopedFakeClock>();
    rtc_fake_clock_->SetTimeMicros(current_time_.us());
  }
}

SimulatedTimeController::~SimulatedTimeController() = default;

Clock* SimulatedTimeController::GetClock() {
  return &sim_clock_;
}

TaskQueueFactory* SimulatedTimeController::GetTaskQueueFactory() {
  return this;
}

std::unique_ptr<TaskQueueBase, TaskQueueDeleter>
SimulatedTimeController::CreateTaskQueue(
    absl::string_view name,
    TaskQueueFactory::Priority priority) const {
  // TODO(srte): Remove the const cast when the interface is made mutable.
  return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
      const_cast<SimulatedTimeController*>(this)->CreateTaskQueue(name));
}

TaskQueueBase* SimulatedTimeController::CreateTaskQueue(
    absl::string_view name) {
  auto* task_queue = new Runner(this, name);
  rtc::CritScope lock(&lock_);
  runners_.push_back(task_queue);
  task_queue->Start();
  return task_queue;
}

std::unique_ptr<ProcessThread> SimulatedTimeController::CreateProcessThread(
    const char* thread_name) {
  rtc::CritScope lock(&lock_);
  auto process_thread = absl::make_unique<Runner>(this, thread_name);
  runners_.push_back(process_thread.get());
  return process_thread;
}

void SimulatedTimeController::Wait(TimeDelta duration) {
  Timestamp target_time = Timestamp::PlusInfinity();
  {
    rtc::CritScope lock(&lock_);
    target_time = current_time_ + duration;
  }
  while (auto* next_runner = AdvanceTimeAndGetNextRunner(target_time)) {
    next_runner->TimeUpdate();
    next_runner->Run();
  }
  rtc::CritScope lock(&lock_);
  TimeDelta delta = target_time - current_time_;
  current_time_ = target_time;
  sim_clock_.AdvanceTimeMicroseconds(delta.us());
  if (rtc_fake_clock_)
    rtc_fake_clock_->AdvanceTime(delta);
}

SimulatedTimeController::Runner*
SimulatedTimeController::AdvanceTimeAndGetNextRunner(Timestamp target_time) {
  rtc::CritScope lock(&lock_);
  if (current_time_ > target_time)
    return nullptr;

  Runner* next_runner = nullptr;
  Timestamp next_time = Timestamp::PlusInfinity();
  for (auto* runner : runners_) {
    Timestamp next_run_time = runner->GetNextRunTime();
    if (next_run_time <= current_time_) {
      return runner;
    } else if (next_run_time < next_time) {
      next_runner = runner;
      next_time = next_run_time;
    }
  }
  if (next_time > target_time) {
    next_time = target_time;
    next_runner = nullptr;
  }

  TimeDelta delta = next_time - current_time_;
  current_time_ = next_time;
  sim_clock_.AdvanceTimeMicroseconds(delta.us());
  if (rtc_fake_clock_)
    rtc_fake_clock_->AdvanceTime(delta);
  return next_runner;
}

void SimulatedTimeController::Unregister(Runner* runner) {
  rtc::CritScope lock(&lock_);
  std::remove(runners_.begin(), runners_.end(), runner);
}

}  // namespace webrtc
