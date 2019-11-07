/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/time_controller/external_time_controller.h"

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
namespace {

class ProcessThreadWrapper : public ProcessThread {
 public:
  ProcessThreadWrapper(ExternalTimeController* parent,
                       std::unique_ptr<ProcessThread> thread)
      : parent_(parent), thread_(std::move(thread)) {}

  void Start() override {
    thread_->Start();
    parent_->ScheduleNext();
  }

  void Stop() override {
    thread_->Stop();
    parent_->ScheduleNext();
  }

  void WakeUp(Module* module) override {
    thread_->WakeUp(GetWrapper(module));
    parent_->ScheduleNext();
  }

  void PostTask(std::unique_ptr<QueuedTask> task) override {
    thread_->PostTask(std::move(task));
    parent_->ScheduleNext();
  }

  void RegisterModule(Module* module, const rtc::Location& from) override {
    module_wrappers_.emplace(module, new ModuleWrapper(module, this));
    thread_->RegisterModule(GetWrapper(module), from);
    parent_->ScheduleNext();
  }

  void DeRegisterModule(Module* module) override {
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

  ExternalTimeController* const parent_;
  std::unique_ptr<ProcessThread> thread_;
  std::map<Module*, std::unique_ptr<ModuleWrapper>> module_wrappers_;
};

class TaskQueueWrapper : public TaskQueueBase {
 public:
  TaskQueueWrapper(ExternalTimeController* parent,
                   std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base)
      : parent_(parent), base_(std::move(base)) {}

  void PostTask(std::unique_ptr<QueuedTask> task) override {
    base_->PostTask(std::make_unique<TaskWrapper>(std::move(task), this));
    parent_->ScheduleNext();
  }

  void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t ms) override {
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

  ExternalTimeController* const parent_;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base_;
};

}  // namespace

ExternalTimeController::ExternalTimeController(
    ExternalController* const controller)
    : controller_(controller), impl_(controller_->GetClock()->CurrentTime()) {}

rtc::YieldInterface* ExternalTimeController::YieldInterface() {
  return &impl_;
}

std::unique_ptr<TaskQueueBase, TaskQueueDeleter>
ExternalTimeController::CreateTaskQueue(
    absl::string_view name,
    TaskQueueFactory::Priority priority) const {
  return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
      new TaskQueueWrapper(const_cast<ExternalTimeController*>(this),
                           impl_.CreateTaskQueue(name, priority)));
}

Clock* ExternalTimeController::GetClock() {
  return controller_->GetClock();
}

TaskQueueFactory* ExternalTimeController::GetTaskQueueFactory() {
  return this;
}

std::unique_ptr<ProcessThread> ExternalTimeController::CreateProcessThread(
    const char* thread_name) {
  return std::make_unique<ProcessThreadWrapper>(
      this, impl_.CreateProcessThread(thread_name));
}

void ExternalTimeController::Sleep(TimeDelta duration) {
  controller_->RunFor(duration);
}

void ExternalTimeController::InvokeWithControlledYield(
    std::function<void()> closure) {
  rtc::ScopedYieldPolicy policy(&impl_);
  closure();
}

void ExternalTimeController::ScheduleNext() {
  if (impl_.NextRunTime().IsFinite()) {
    controller_->ScheduleAt(impl_.NextRunTime());
  }
}

void ExternalTimeController::Run() {
  rtc::ScopedYieldPolicy yield_policy(&impl_);
  impl_.AdvanceTime(controller_->GetClock()->CurrentTime());
  impl_.RunReadyRunners();

  ScheduleNext();
}

}  // namespace webrtc
