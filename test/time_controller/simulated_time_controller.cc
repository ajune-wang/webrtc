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
namespace {

ABSL_CONST_INIT thread_local bool is_invoking = false;

class ScopedInvokeState {
 public:
  ScopedInvokeState() : previous_(is_invoking) { is_invoking = true; }
  ScopedInvokeState(const ScopedInvokeState&) = delete;
  ScopedInvokeState& operator=(const ScopedInvokeState&) = delete;
  ~ScopedInvokeState() { is_invoking = previous_; }

 private:
  const bool previous_;
};

rtc::ScopedFakeClock CreateScopedClock(Timestamp start_time) {
  rtc::ScopedFakeClock clock;
  clock.SetTimeMicros(start_time.us());
  return clock;
}
}  // namespace

namespace sim_time_impl {
class SimulatedSequenceRunner : public ProcessThread, public TaskQueueBase {
 public:
  SimulatedSequenceRunner(SimulatedTimeControllerImpl* handler,
                          absl::string_view queue_name)
      : handler_(handler), name_(queue_name) {}
  ~SimulatedSequenceRunner() override { handler_->Unregister(this); }

  Timestamp GetNextRunTime() const;

  void TimeUpdate(Timestamp at_time);
  void Run(Timestamp at_time);
  bool InvokeTask(QueuedTask* task);

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
  Timestamp GetCurrentTime() const { return handler_->CurrentTime(); }
  void RunPendingTasks(Timestamp at_time) RTC_LOCKS_EXCLUDED(lock_);
  void RunPendingModules(Timestamp at_time) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void UpdateProcessTime() RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  SimulatedTimeControllerImpl* const handler_;
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

void SimulatedSequenceRunner::TimeUpdate(Timestamp at_time) {
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

void SimulatedSequenceRunner::Run(Timestamp at_time) {
  RunPendingTasks(at_time);
  rtc::CritScope lock(&lock_);
  RunPendingModules(at_time);
  UpdateProcessTime();
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

bool SimulatedSequenceRunner::InvokeTask(QueuedTask* task) {
  CurrentTaskQueueSetter set_current(this);
  RunPendingTasks(GetCurrentTime());
  bool delete_task = task->Run();
  rtc::CritScope lock(&lock_);
  UpdateProcessTime();
  return delete_task;
}
void SimulatedSequenceRunner::PostTask(std::unique_ptr<QueuedTask> task) {
  // There are situations where PostTask is used to create a blocking invoke
  // call using rtc::Event. If we would just post the task without executing it
  // here this would cause a dead lock. If a task is posted from the same thread
  // as the task queue but not from a task queue we have to assume that
  // this might be the situation and process all currently enqueued tasks up to
  // and including the newly posted task. Note that this breaks synchronisation
  // guarantees if the task posts a task back to the orginating task queue.
  if (is_invoking || (Current() == nullptr && handler_->OnCurrentThread())) {
    if (!InvokeTask(task.get()))
      task.release();
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

SimulatedTimeControllerImpl::SimulatedTimeControllerImpl(Timestamp start_time)
    : thread_id_(std::this_thread::get_id()), current_time_(start_time) {}

SimulatedTimeControllerImpl::~SimulatedTimeControllerImpl() = default;

std::unique_ptr<TaskQueueBase, TaskQueueDeleter>
SimulatedTimeControllerImpl::CreateTaskQueue(
    absl::string_view name,
    TaskQueueFactory::Priority priority) const {
  // TODO(srte): Remove the const cast when the interface is made mutable.
  auto mutable_this = const_cast<SimulatedTimeControllerImpl*>(this);
  auto task_queue = std::unique_ptr<SimulatedSequenceRunner, TaskQueueDeleter>(
      new SimulatedSequenceRunner(mutable_this, name));
  rtc::CritScope lock(&mutable_this->lock_);
  mutable_this->runners_.insert(task_queue.get());
  return task_queue;
}

std::unique_ptr<ProcessThread> SimulatedTimeControllerImpl::CreateProcessThread(
    const char* thread_name) {
  rtc::CritScope lock(&lock_);
  auto process_thread =
      absl::make_unique<SimulatedSequenceRunner>(this, thread_name);
  runners_.insert(process_thread.get());
  return process_thread;
}

std::vector<SimulatedSequenceRunner*> SimulatedTimeControllerImpl::GetPending(
    Timestamp current_time) {
  rtc::CritScope lock(&lock_);
  std::vector<SimulatedSequenceRunner*> pending;
  for (auto* runner : runners_) {
    if (runner->GetNextRunTime() <= current_time)
      pending.push_back(runner);
  }
  return pending;
}

bool SimulatedTimeControllerImpl::HasRunner(SimulatedSequenceRunner* runner) {
  rtc::CritScope lock(&lock_);
  return runners_.find(runner) != runners_.end();
}

void SimulatedTimeControllerImpl::RunPending() {
  Timestamp current_time = CurrentTime();
  // We repeat until we have no pending left to handle tasks posted by pending
  // runners.
  while (true) {
    auto pending = GetPending(current_time);
    if (pending.empty())
      break;
    for (auto* runner : pending) {
      runner->TimeUpdate(current_time);
      runner->Run(current_time);
    }
  }
}

Timestamp SimulatedTimeControllerImpl::CurrentTime() const {
  rtc::CritScope lock(&time_lock_);
  return current_time_;
}

TimeDelta SimulatedTimeControllerImpl::AdvanceTime(Timestamp limit) {
  Timestamp current_time = CurrentTime();
  Timestamp next_time = limit;
  rtc::CritScope lock(&lock_);
  for (auto* runner : runners_) {
    Timestamp next_run_time = runner->GetNextRunTime();
    if (next_run_time <= current_time)
      return TimeDelta::Zero();
    next_time = std::min(next_time, next_run_time);
  }
  rtc::CritScope time_lock(&time_lock_);
  current_time_ = next_time;
  return next_time - current_time;
}

void SimulatedTimeControllerImpl::Unregister(SimulatedSequenceRunner* runner) {
  rtc::CritScope lock(&lock_);
  RTC_CHECK(runners_.erase(runner));
}

std::function<void(TaskQueueBase*, QueuedTask*)>
SimulatedTimeControllerImpl::TaskInvoker() {
  return [this](TaskQueueBase* task_queue, QueuedTask* task) {
    ScopedInvokeState invoke_state;
    auto* runner = static_cast<SimulatedSequenceRunner*>(task_queue);
    RTC_DCHECK(HasRunner(runner));
    bool delete_task = runner->InvokeTask(task);
    RTC_DCHECK(delete_task);
  };
}

}  // namespace sim_time_impl

SimulatedTimeController::SimulatedTimeController(Timestamp start_time)
    : sim_clock_(start_time.us()), impl_(start_time) {}

Clock* SimulatedTimeController::GetClock() {
  return &sim_clock_;
}

TaskQueueFactory* SimulatedTimeController::GetTaskQueueFactory() {
  return &impl_;
}

std::unique_ptr<ProcessThread> SimulatedTimeController::CreateProcessThread(
    const char* thread_name) {
  return impl_.CreateProcessThread(thread_name);
}

void SimulatedTimeController::Sleep(TimeDelta duration) {
  Timestamp target_time = impl_.CurrentTime() + duration;
  RTC_DCHECK_EQ(impl_.CurrentTime().us(), sim_clock_.TimeInMicroseconds());
  while (sim_clock_.TimeInMicroseconds() < target_time.us()) {
    impl_.RunPending();
    auto delta = impl_.AdvanceTime(target_time);
    sim_clock_.AdvanceTimeMicroseconds(delta.us());
  }
}

std::function<void(TaskQueueBase*, QueuedTask*)>
SimulatedTimeController::TaskInvoker() {
  return impl_.TaskInvoker();
}

GlobalSimulatedTimeController::GlobalSimulatedTimeController(
    Timestamp start_time)
    : global_clock_(CreateScopedClock(start_time)),
      sim_clock_(start_time.us()),
      impl_(start_time) {}

Clock* GlobalSimulatedTimeController::GetClock() {
  return &sim_clock_;
}

TaskQueueFactory* GlobalSimulatedTimeController::GetTaskQueueFactory() {
  return &impl_;
}

std::unique_ptr<ProcessThread>
GlobalSimulatedTimeController::CreateProcessThread(const char* thread_name) {
  return impl_.CreateProcessThread(thread_name);
}

void GlobalSimulatedTimeController::Sleep(TimeDelta duration) {
  Timestamp current_time = impl_.CurrentTime();
  Timestamp target_time = current_time + duration;
  RTC_DCHECK_EQ(current_time.us(), rtc::TimeMicros());
  while (current_time < target_time) {
    impl_.RunPending();
    auto delta = impl_.AdvanceTime(target_time);
    current_time += delta;
    sim_clock_.AdvanceTimeMicroseconds(delta.us());
    global_clock_.AdvanceTimeMicros(delta.us());
  }
}

std::function<void(TaskQueueBase*, QueuedTask*)>
GlobalSimulatedTimeController::TaskInvoker() {
  return impl_.TaskInvoker();
}

// namespace sim_time_impl

}  // namespace webrtc
