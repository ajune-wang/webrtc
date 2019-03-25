/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/time_controller/real_time_controller.h"

#include "api/task_queue/global_task_queue_factory.h"
#include "rtc_base/event.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "system_wrappers/include/sleep.h"
#include "test/single_threaded_task_queue.h"

namespace webrtc {

namespace webrtc_impl {
namespace {
class SingleThreadedTaskQueue : public TaskQueueBase {
 public:
  explicit SingleThreadedTaskQueue(absl::string_view name)
      : impl_(std::string(name).c_str()) {}
  ~SingleThreadedTaskQueue() override = default;
  void Delete() override { delete this; }
  void PostTask(std::unique_ptr<QueuedTask> task) override {
    auto task_ptr = task.release();
    impl_.PostTask([task_ptr] {
      if (task_ptr->Run())
        delete task_ptr;
    });
  }
  void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                       uint32_t milliseconds) override {
    auto task_ptr = task.release();
    impl_.PostDelayedTask(
        [task_ptr] {
          if (task_ptr->Run())
            delete task_ptr;
        },
        milliseconds);
  }

 private:
  test::SingleThreadedTaskQueueForTesting impl_;
};
}  // namespace
std::unique_ptr<TaskQueueBase, TaskQueueDeleter>
SingleThreadedTaskQueueFactory::CreateTaskQueue(
    absl::string_view name,
    TaskQueueFactory::Priority priority) const {
  return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
      new SingleThreadedTaskQueue(name));
}

}  // namespace webrtc_impl

Clock* RealTimeController::GetClock() {
  return Clock::GetRealTimeClock();
}

TaskQueueFactory* RealTimeController::GetTaskQueueFactory() {
  return &GlobalTaskQueueFactory();
}

TaskQueueFactory* RealTimeController::GetSingleThreadedTaskQueueFactory() {
  return &single_threaded_task_queue_factory_;
}

std::unique_ptr<ProcessThread> RealTimeController::CreateProcessThread(
    const char* thread_name) {
  return ProcessThread::Create(thread_name);
}

void RealTimeController::Sleep(TimeDelta duration) {
  SleepMs(duration.ms());
}

void RealTimeController::InvokeWithControlledYield(
    std::function<void()> closure) {
  closure();
}

}  // namespace webrtc
