/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/quantum_task_queue_factory.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {

class QuantumTaskQueue : public TaskQueueBase {
 public:
  QuantumTaskQueue(
      std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base_task_queue,
      TimeDelta quantum_delay,
      Clock* clock)
      : base_task_queue_(std::move(base_task_queue)),
        epoch_(clock->CurrentTime()),
        quantum_delay_(quantum_delay),
        clock_(clock) {}

  void Delete() override { delete this; }

  void PostTask(std::unique_ptr<QueuedTask> task) override {
    base_task_queue_->PostTask(std::move(task));
  }

  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override {
    Timestamp now = clock_->CurrentTime();
    TimeDelta fire_duration_since_epoch =
        now + TimeDelta::Millis(milliseconds) - epoch_;
    int64_t rounded_quantums_since_epoch =
        (fire_duration_since_epoch + /* round upwards */ quantum_delay_ -
         TimeDelta::Micros(1)) /
        quantum_delay_;
    int64_t adjusted_millis =
        (epoch_ + quantum_delay_ * rounded_quantums_since_epoch - now).ms();
    base_task_queue_->PostDelayedTask(
        std::move(task), adjusted_millis < 0 ? 0 : adjusted_millis);
  }

 private:
  const std::unique_ptr<TaskQueueBase, TaskQueueDeleter> base_task_queue_;
  const Timestamp epoch_;
  const TimeDelta quantum_delay_;
  Clock* const clock_;
};

class QuantumTaskQueueFactory : public TaskQueueFactory {
 public:
  QuantumTaskQueueFactory(TaskQueueFactory* base_task_queue_factory,
                          TimeDelta quantum_delay,
                          Clock* clock)
      : base_task_queue_factory_(base_task_queue_factory),
        quantum_delay_(quantum_delay),
        clock_(clock) {}

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override {
    return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
        new QuantumTaskQueue(
            base_task_queue_factory_->CreateTaskQueue(name, priority),
            quantum_delay_, clock_));
  }

 private:
  TaskQueueFactory* const base_task_queue_factory_;
  const TimeDelta quantum_delay_;
  Clock* const clock_;
};

}  // namespace

std::unique_ptr<TaskQueueFactory> CreateQuantumTaskQueueFactory(
    TaskQueueFactory* base_task_queue_factory,
    TimeDelta quantum_delay,
    Clock* clock) {
  return std::make_unique<QuantumTaskQueueFactory>(base_task_queue_factory,
                                                   quantum_delay, clock);
}

}  // namespace webrtc
