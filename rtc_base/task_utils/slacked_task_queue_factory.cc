/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/slacked_task_queue_factory.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {

// This class exists to ensure that DelayedCallQueue can safely access the
// queue, whose lifetime is shorter than the queue.
struct TaskQueueHolder : public rtc::RefCountedNonVirtual<TaskQueueHolder> {
  explicit TaskQueueHolder(
      std::unique_ptr<TaskQueueBase, TaskQueueDeleter> queue)
      : queue(std::move(queue)) {}

  void MarkDead() RTC_LOCKS_EXCLUDED(mu_) {
    MutexLock lock(&mu_);
    queue = nullptr;
  }

  bool Alive() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    return queue != nullptr;
  }

  Mutex mu_;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> queue RTC_GUARDED_BY(mu_);
};

class DelayedCallQueue : public rtc::RefCountedNonVirtual<DelayedCallQueue> {
 public:
  DelayedCallQueue(
      std::unique_ptr<TaskQueueBase, TaskQueueDeleter> manager_task_queue,
      std::unique_ptr<DelayedCallProvider> provider,
      Clock* clock)
      : task_queue_(std::move(manager_task_queue)),
        delayed_call_provider_(std::move(provider)),
        clock_(clock) {
    sequence_.Detach();
  }

  ~DelayedCallQueue() { RTC_DCHECK_RUN_ON(&sequence_); }

  void ReleaseOnManagerTaskQueue() {
    // REVIEWER COMMENT. The point of this dance is to ensure ScopedTaskSafety
    // gets destroyed on the right task queue. However, it seems that I can't
    // destroy a task queue from it's own sequence, so to ensure the final
    // Release happens on the task queue, I wait synchronously for the lambda
    // to complete.
    //
    // Is there a better way of doing this? An alternative is to not use an
    // explicit task queue, but re-use the one the DelayedCallQueue ctor is
    // called on. However, this fails the task queue test which does synchronous
    // waits from that context...
    if (!HasOneRef()) {
      Release();
      return;
    }
    std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue =
        std::move(task_queue_);
    rtc::Event done;
    task_queue->PostTask(ToQueuedTask([this, &done] {
      // Reference is dropped on task_queue_, and we're deleted here eventually.
      RTC_DCHECK_RUN_ON(&sequence_);
      Release();
      done.Set();
    }));
    done.Wait(rtc::Event::kForever);
  }

  void ScheduleDelayedTask(rtc::scoped_refptr<TaskQueueHolder> holder,
                           std::unique_ptr<QueuedTask> task,
                           uint32_t millis) RTC_LOCKS_EXCLUDED(sequence_) {
    DelayedTask delayed_task{holder, std::move(task),
                             clock_->CurrentTime() + TimeDelta::Millis(millis)};
    task_queue_->PostTask(
        ToQueuedTask(safety_.flag(),
                     [this, delayed_task = std::move(delayed_task)]() mutable {
                       RTC_DCHECK_RUN_ON(&sequence_);
                       PushDelayedTask(std::move(delayed_task));
                       MaybeScheduleNextWakeup(clock_->CurrentTime());
                     }));
  }

 private:
  struct DelayedTask {
    rtc::scoped_refptr<TaskQueueHolder> holder;
    std::unique_ptr<QueuedTask> task;
    Timestamp fire_time;
  };
  struct CompareDelayedTask {
    bool operator()(const DelayedTask& a, const DelayedTask& b) const {
      return a.fire_time > b.fire_time;
    }
  };

  void RunDelayedTask(DelayedTask delayed_task) RTC_RUN_ON(sequence_) {
    MutexLock lock(&delayed_task.holder->mu_);
    if (delayed_task.holder->Alive()) {
      delayed_task.holder->queue->PostTask(
          ToQueuedTask([holder = delayed_task.holder,
                        task = std::move(delayed_task.task)]() mutable {
            MutexLock lock(&holder->mu_);
            if (!holder->Alive())
              return;
            if (!task->Run())
              (void)task.release();
          }));
    }
  }

  Timestamp DepleteRipeTasks() RTC_RUN_ON(sequence_) {
    Timestamp now = clock_->CurrentTime();
    int64_t count = 0;
    while (!q_.empty() && PeekTopDelayedTask().fire_time <= now) {
      count++;
      RunDelayedTask(PopDelayedTask());
    }
    if (count) {
      RTC_DLOG_V(rtc::LS_INFO) << "Triggered " << count << " tasks.";
    }
    return now;
  }

  void MaybeScheduleNextWakeup(Timestamp now) RTC_RUN_ON(sequence_) {
    if (q_.empty()) {
      next_wakeup_ = absl::nullopt;
      return;
    }
    // If we're here due to posting a delayed call after our current scheduled
    // wakeup time, there's no need to reschedule anything.
    if (next_wakeup_.has_value() &&
        *next_wakeup_ <= PeekTopDelayedTask().fire_time) {
      return;
    }
    // !next_wakeup.has_value() || *next_wakeup_ >
    // PeekTopDelayedTask().quantized_fire_time
    //
    // Need to adjust or set the wakeup time.
    // Cancel any old delayed calls in flight by incrementing [epoch_|.
    epoch_++;
    next_wakeup_ = PeekTopDelayedTask().fire_time;
    int64_t millisecond_delay =
        (*next_wakeup_ - now + TimeDelta::Millis(1) - TimeDelta::Micros(1))
            .us() /
        rtc::kNumMicrosecsPerMillisec;
    delayed_call_provider_->ScheduleDelayedCall(
        ToQueuedTask(safety_.flag(),
                     [this, epoch = epoch_] {
                       RTC_DCHECK_RUN_ON(&sequence_);
                       // Detect cancelled call and return early if so.
                       if (epoch < epoch_)
                         return;
                       next_wakeup_ = absl::nullopt;
                       MaybeScheduleNextWakeup(DepleteRipeTasks());
                     }),
        millisecond_delay);
  }

  void PushDelayedTask(DelayedTask delayed_task) RTC_RUN_ON(sequence_) {
    q_.push_back(std::move(delayed_task));
    std::push_heap(q_.begin(), q_.end(), CompareDelayedTask());
  }

  DelayedTask PopDelayedTask() RTC_RUN_ON(sequence_) {
    RTC_DCHECK(!q_.empty());
    std::pop_heap(q_.begin(), q_.end(), CompareDelayedTask());
    DelayedTask delayed_task = std::move(q_.back());
    q_.pop_back();
    return delayed_task;
  }

  const DelayedTask& PeekTopDelayedTask() const RTC_RUN_ON(sequence_) {
    RTC_DCHECK(!q_.empty());
    return q_[0];
  }

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_;
  const std::unique_ptr<DelayedCallProvider> delayed_call_provider_;
  Clock* const clock_;

  std::vector<DelayedTask> q_ RTC_GUARDED_BY(sequence_);
  absl::optional<Timestamp> next_wakeup_ RTC_GUARDED_BY(sequence_);
  // Used for detecting old delayed calls which we don't want to consider
  // anymore.
  int64_t epoch_ RTC_GUARDED_BY(sequence_) = 0;

  ScopedTaskSafetyDetached safety_;
};

class SlackedTaskQueue : public TaskQueueBase {
 public:
  SlackedTaskQueue(
      rtc::scoped_refptr<DelayedCallQueue> delayed_call_queue,
      std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base_task_queue)
      : delayed_call_queue_(std::move(delayed_call_queue)),
        holder_(new TaskQueueHolder(std::move(base_task_queue))),
        base_task_queue_(holder_->queue.get()) {}

  ~SlackedTaskQueue() {
    // REVIEWER COMMENT: better way of doing this? Need to drop the ref on the
    // manager task queue.
    DelayedCallQueue* delayed_call_queue = delayed_call_queue_.release();
    delayed_call_queue->ReleaseOnManagerTaskQueue();
    MutexLock lock(&holder_->mu_);
    holder_->queue = nullptr;
  }

  void Delete() override { delete this; }

  void PostTask(std::unique_ptr<QueuedTask> task) override {
    base_task_queue_->PostTask(
        std::make_unique<SlackedQueuedTask>(this, std::move(task)));
  }

  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override {
    task = std::make_unique<SlackedQueuedTask>(this, std::move(task));
    // if (milliseconds == 0) {
    //   base_task_queue_->PostTask(std::move(task));
    //   return;
    // }

    delayed_call_queue_->ScheduleDelayedTask(holder_, std::move(task),
                                             milliseconds);
  }

 private:
  class SlackedQueuedTask : public QueuedTask {
   public:
    SlackedQueuedTask(SlackedTaskQueue* task_queue,
                      std::unique_ptr<QueuedTask> task)
        : task_queue_(task_queue), task_(std::move(task)) {}
    bool Run() override {
      CurrentTaskQueueSetter setter(task_queue_);
      if (!task_->Run())
        (void)task_.release();
      return true;
    }

   private:
    SlackedTaskQueue* const task_queue_;
    std::unique_ptr<QueuedTask> task_;
  };

  rtc::scoped_refptr<DelayedCallQueue> delayed_call_queue_;
  const rtc::scoped_refptr<TaskQueueHolder> holder_;
  TaskQueueBase* const base_task_queue_;
};

class SlackedTaskQueueFactory : public TaskQueueFactory {
 public:
  SlackedTaskQueueFactory(
      rtc::scoped_refptr<DelayedCallQueue> delayed_call_queue,
      TaskQueueFactory* base_task_queue_factory)
      : delayed_call_queue_(delayed_call_queue),
        base_task_queue_factory_(base_task_queue_factory) {}

  ~SlackedTaskQueueFactory() {
    // REVIEWER COMMENT: better way of doing this? Need to drop the ref on the
    // manager task queue.
    DelayedCallQueue* delayed_call_queue = delayed_call_queue_.release();
    delayed_call_queue->ReleaseOnManagerTaskQueue();
  }

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override {
    return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
        new SlackedTaskQueue(
            delayed_call_queue_,
            base_task_queue_factory_->CreateTaskQueue(name, priority)));
  }

 private:
  rtc::scoped_refptr<DelayedCallQueue> delayed_call_queue_;
  TaskQueueFactory* const base_task_queue_factory_;
};

class QuantumDelayedCallProvider : public DelayedCallProvider {
 public:
  QuantumDelayedCallProvider(Clock* clock, TimeDelta quantum)
      : clock_(clock),
        creation_time_(clock_->CurrentTime()),
        quantum_(quantum) {
    sequence_.Detach();
  }

  // DelayedCallProvider.
  void ScheduleDelayedCall(std::unique_ptr<QueuedTask> task,
                           uint32_t milliseconds) override {
    RTC_DCHECK_RUN_ON(&sequence_);
    Timestamp now = clock_->CurrentTime();
    int64_t quantum_index =
        (now - creation_time_ + TimeDelta::Millis(milliseconds) + quantum_ -
         TimeDelta::Micros(1)) /
        quantum_;
    Timestamp fire_time = creation_time_ + quantum_index * quantum_;
    int64_t delay_milliseconds =
        (fire_time - now + TimeDelta::Millis(1) - TimeDelta::Micros(1)).us() /
        rtc::kNumMicrosecsPerMillisec;
    TaskQueueBase::Current()->PostDelayedTask(
        ToQueuedTask(safety_.flag(),
                     [clock = clock_, this, task = std::move(task)]() mutable {
                       RTC_DLOG_V(rtc::LS_INFO)
                           << this
                           << " Trigger, now = " << clock->CurrentTime().ms();
                       if (!task->Run())
                         (void)task.release();
                     }),
        delay_milliseconds);
  }

 private:
  Clock* const clock_;
  const Timestamp creation_time_;
  const TimeDelta quantum_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_;
  ScopedTaskSafetyDetached safety_;
};

}  // namespace

std::unique_ptr<TaskQueueFactory> CreateSlackedTaskQueueFactory(
    TaskQueueFactory* base_task_queue_factory,
    std::unique_ptr<DelayedCallProvider> provider,
    Clock* clock) {
  return std::make_unique<SlackedTaskQueueFactory>(
      new DelayedCallQueue(
          base_task_queue_factory->CreateTaskQueue(
              "SlackedManager", TaskQueueFactory::Priority::NORMAL),
          std::move(provider), clock),
      base_task_queue_factory);
}

std::unique_ptr<DelayedCallProvider> CreateQuantumDelayedCallProvider(
    Clock* clock,
    TimeDelta quantum) {
  return std::make_unique<QuantumDelayedCallProvider>(clock, quantum);
}

}  // namespace webrtc
