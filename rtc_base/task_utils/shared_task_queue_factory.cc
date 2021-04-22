/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/shared_task_queue_factory.h"

#include <memory>
#include <utility>

#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace {

class SharedTaskQueue : public TaskQueueBase {
 public:
  explicit SharedTaskQueue(
      rtc::scoped_refptr<rtc::RefCountedObject<
          std::unique_ptr<TaskQueueBase, TaskQueueDeleter>>> shared_task_queue)
      : shared_task_queue_(shared_task_queue), state_(new State) {
    state_->owning_task_queue = this;
  }

  ~SharedTaskQueue() override {
    // Ensures TaskQueueBase::Delete semantics.
    webrtc::MutexLock lock(&state_->mutex);
    state_->status = State::Status::kDead;
  }

  void Delete() override { delete this; }

  void PostTask(std::unique_ptr<QueuedTask> task) override {
    (*shared_task_queue_)
        ->PostTask(
            std::make_unique<QueuedTaskWrapper>(std::move(task), state_));
  }

  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override {
    (*shared_task_queue_)
        ->PostDelayedTask(
            std::make_unique<QueuedTaskWrapper>(std::move(task), state_),
            milliseconds);
  }

  static bool RunTaskOnSharedTaskQueue(QueuedTask* task,
                                       TaskQueueBase* task_queue) {
    CurrentTaskQueueSetter setter(task_queue);
    return task->Run();
  }

 private:
  struct State : public rtc::RefCountedNonVirtual<State> {
    webrtc::Mutex mutex;
    enum class Status {
      kLive,
      kDead
    } status RTC_GUARDED_BY(mutex) = Status::kLive;
    TaskQueueBase* owning_task_queue = nullptr;
  };

  class QueuedTaskWrapper : public QueuedTask {
   public:
    QueuedTaskWrapper(std::unique_ptr<QueuedTask> task,
                      rtc::scoped_refptr<State> state)
        : task_(std::move(task)), state_(state) {
      RTC_DCHECK(task_);
    }

    bool Run() override {
      webrtc::MutexLock lock(&state_->mutex);
      if (state_->status == State::Status::kLive) {
        if (SharedTaskQueue::RunTaskOnSharedTaskQueue(
                task_.get(), state_->owning_task_queue)) {
          task_ = nullptr;
        } else {
          (void)task_.release();
        }
      }
      return true;
    }

   private:
    std::unique_ptr<QueuedTask> task_;
    rtc::scoped_refptr<State> state_;
  };

  rtc::scoped_refptr<
      rtc::RefCountedObject<std::unique_ptr<TaskQueueBase, TaskQueueDeleter>>>
      shared_task_queue_;
  rtc::scoped_refptr<State> state_;
};

class SharedTaskQueueFactory : public TaskQueueFactory {
 public:
  explicit SharedTaskQueueFactory(TaskQueueFactory* base_task_queue_factory)
      : base_task_queue_factory_(base_task_queue_factory) {}

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override {
    webrtc::MutexLock lock(&mutex_);
    if (!shared_task_queue_) {
      auto task_queue =
          base_task_queue_factory_->CreateTaskQueue(name, priority);
      shared_task_queue_ = new rtc::RefCountedObject<
          std::unique_ptr<TaskQueueBase, TaskQueueDeleter>>(
          std::move(task_queue));
    }
    return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
        new SharedTaskQueue(shared_task_queue_));
  }

 private:
  mutable webrtc::Mutex mutex_;
  TaskQueueFactory* const base_task_queue_factory_ RTC_GUARDED_BY(mutex_);
  mutable rtc::scoped_refptr<
      rtc::RefCountedObject<std::unique_ptr<TaskQueueBase, TaskQueueDeleter>>>
      shared_task_queue_ RTC_GUARDED_BY(mutex_);
};

}  // namespace

std::unique_ptr<TaskQueueFactory> CreateSharedTaskQueueFactory(
    TaskQueueFactory* base_task_queue_factory) {
  return std::make_unique<SharedTaskQueueFactory>(base_task_queue_factory);
}

}  // namespace webrtc
