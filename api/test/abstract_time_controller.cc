/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/abstract_time_controller.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/include/module.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/synchronization/yield_policy.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

class AbstractTimeController::ProcessThreadWrapper : public ProcessThread {
 public:
  ProcessThreadWrapper(AbstractTimeController* parent,
                       std::unique_ptr<ProcessThread> thread)
      : parent_(parent), thread_(std::move(thread)) {}

  void Start() override {
    parent_->UpdateTime();
    thread_->Start();
    parent_->ScheduleNext();
  }

  void Stop() override {
    parent_->UpdateTime();
    thread_->Stop();
    parent_->ScheduleNext();
  }

  void WakeUp(Module* module) override {
    parent_->UpdateTime();
    thread_->WakeUp(GetWrapper(module));
    parent_->ScheduleNext();
  }

  void PostTask(std::unique_ptr<QueuedTask> task) override {
    parent_->UpdateTime();
    thread_->PostTask(std::move(task));
    parent_->ScheduleNext();
  }

  void RegisterModule(Module* module, const rtc::Location& from) override {
    parent_->UpdateTime();
    module_wrappers_.emplace(module, new ModuleWrapper(module, this));
    thread_->RegisterModule(GetWrapper(module), from);
    parent_->ScheduleNext();
  }

  void DeRegisterModule(Module* module) override {
    parent_->UpdateTime();
    thread_->DeRegisterModule(GetWrapper(module));
    parent_->ScheduleNext();
    module_wrappers_.erase(module);
  }

 private:
  class ModuleWrapper : public Module {
   public:
    ModuleWrapper(Module* module, ProcessThreadWrapper* thread)
        : module_(module), thread_(thread) {}

    int64_t TimeUntilNextProcess() override {
      return module_->TimeUntilNextProcess();
    }

    void Process() override { module_->Process(); }

    void ProcessThreadAttached(ProcessThread* process_thread) override {
      if (process_thread) {
        module_->ProcessThreadAttached(thread_);
      } else {
        module_->ProcessThreadAttached(nullptr);
      }
    }

   private:
    Module* module_;
    ProcessThreadWrapper* thread_;
  };

  ModuleWrapper* GetWrapper(Module* module) {
    auto it = module_wrappers_.find(module);
    RTC_DCHECK(it != module_wrappers_.end());
    return it->second.get();
  }

  AbstractTimeController* const parent_;
  std::unique_ptr<ProcessThread> thread_;
  std::map<Module*, std::unique_ptr<ModuleWrapper>> module_wrappers_;
};

class AbstractTimeController::TaskQueueWrapper : public TaskQueueBase {
 public:
  TaskQueueWrapper(AbstractTimeController* parent,
                   std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base)
      : parent_(parent), base_(std::move(base)) {}

  void PostTask(std::unique_ptr<QueuedTask> task) override {
    parent_->UpdateTime();
    base_->PostTask(std::make_unique<TaskWrapper>(std::move(task), this));
    parent_->ScheduleNext();
  }

  void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t ms) override {
    parent_->UpdateTime();
    base_->PostDelayedTask(std::make_unique<TaskWrapper>(std::move(task), this),
                           ms);
    parent_->ScheduleNext();
  }

  void Delete() override { delete this; }

 private:
  class TaskWrapper : public QueuedTask {
   public:
    TaskWrapper(std::unique_ptr<QueuedTask> task, TaskQueueWrapper* queue)
        : task_(std::move(task)), queue_(queue) {}

    bool Run() override {
      CurrentTaskQueueSetter current(queue_);
      if (!task_->Run()) {
        task_.release();
      }
      // The wrapper should always be deleted, even if it releases the inner
      // task, in order to avoid leaking wrappers.
      return true;
    }

   private:
    std::unique_ptr<QueuedTask> task_;
    TaskQueueWrapper* queue_;
  };

  AbstractTimeController* const parent_;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base_;
};

AbstractTimeController::AbstractTimeController(Clock* clock)
    : clock_(clock), impl_(clock_->CurrentTime()) {
  global_clock_.SetTime(clock_->CurrentTime());
}

Clock* AbstractTimeController::GetClock() {
  return clock_;
}

TaskQueueFactory* AbstractTimeController::GetTaskQueueFactory() {
  return this;
}

std::unique_ptr<ProcessThread> AbstractTimeController::CreateProcessThread(
    const char* thread_name) {
  return std::make_unique<ProcessThreadWrapper>(
      this, impl_.CreateProcessThread(thread_name));
}

void AbstractTimeController::Sleep(TimeDelta duration) {
  RunFor(duration);
}

void AbstractTimeController::InvokeWithControlledYield(
    std::function<void()> closure) {
  rtc::ScopedYieldPolicy policy(&impl_);
  closure();
}

std::unique_ptr<TaskQueueBase, TaskQueueDeleter>
AbstractTimeController::CreateTaskQueue(
    absl::string_view name,
    TaskQueueFactory::Priority priority) const {
  return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
      new TaskQueueWrapper(const_cast<AbstractTimeController*>(this),
                           impl_.CreateTaskQueue(name, priority)));
}

void AbstractTimeController::Run() {
  rtc::ScopedYieldPolicy yield_policy(&impl_);
  UpdateTime();
  impl_.RunReadyRunners();
  ScheduleNext();
}

void AbstractTimeController::UpdateTime() {
  Timestamp now = clock_->CurrentTime();
  impl_.AdvanceTime(now);
  global_clock_.SetTime(now);
}

void AbstractTimeController::ScheduleNext() {
  RTC_DCHECK_EQ(impl_.CurrentTime(), clock_->CurrentTime());
  TimeDelta delay =
      std::max(impl_.NextRunTime() - impl_.CurrentTime(), TimeDelta::Zero());
  if (delay.IsFinite()) {
    ScheduleAt(clock_->CurrentTime() + delay);
  }
}

}  // namespace webrtc
