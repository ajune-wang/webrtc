/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <memory>

#include "absl/memory/memory.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

// NOTE: Since these tests rely on real time behavior, they will be flaky
// if run on heavily loaded systems.
namespace webrtc {
namespace {
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;
constexpr Timestamp kStartTime = Timestamp::Seconds<1000>();

class TaskHandleStoppper {
 public:
  explicit TaskHandleStoppper(RepeatingTaskHandle handle)
      : handle_(std::move(handle)) {}
  void operator()() { handle_.Stop(); }

 private:
  RepeatingTaskHandle handle_;
};
}  // namespace

TEST(TimeSimulationTaskRunnerTest, TaskIsStoppedOnStop) {
  const TimeDelta kShortInterval = TimeDelta::ms(5);
  const TimeDelta kLongInterval = TimeDelta::ms(20);
  const int kShortIntervalCount = 4;
  const int kMargin = 1;
  SimulatedTimeController time_simulation(kStartTime, true);
  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));
  std::atomic_int counter(0);
  auto handle = RepeatingTaskHandle::Start(task_queue.Get(), [&] {
    if (++counter >= kShortIntervalCount)
      return kLongInterval;
    return kShortInterval;
  });
  // Wait long enough to go through the initial phase.
  time_simulation.Wait(kShortInterval * (kShortIntervalCount + kMargin));
  EXPECT_EQ(counter.load(), kShortIntervalCount);

  task_queue.PostTask(TaskHandleStoppper(std::move(handle)));
  // Wait long enough that the task would run at least once more if not
  // stopped.
  time_simulation.Wait(kLongInterval * 2);
  EXPECT_EQ(counter.load(), kShortIntervalCount);
}

TEST(TimeSimulationTaskRunnerTest, TaskCanStopItself) {
  std::atomic_int counter(0);
  SimulatedTimeController time_simulation(kStartTime, true);
  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));

  RepeatingTaskHandle handle;
  task_queue.PostTask([&] {
    handle = RepeatingTaskHandle::Start(task_queue.Get(), [&] {
      ++counter;
      handle.Stop();
      return TimeDelta::ms(2);
    });
  });
  time_simulation.Wait(TimeDelta::ms(10));
  EXPECT_EQ(counter.load(), 1);
}
TEST(TimeSimulationTaskRunnerTest, Example) {
  class ObjectOnTaskQueue {
   public:
    void DoPeriodicTask() {}
    TimeDelta TimeUntilNextRun() { return TimeDelta::ms(100); }
    void StartPeriodicTask(RepeatingTaskHandle* handle,
                           rtc::TaskQueue* task_queue) {
      *handle = RepeatingTaskHandle::Start(task_queue->Get(), [this] {
        DoPeriodicTask();
        return TimeUntilNextRun();
      });
    }
  };
  SimulatedTimeController time_simulation(kStartTime, true);
  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));
  auto object = absl::make_unique<ObjectOnTaskQueue>();
  // Create and start the periodic task.
  RepeatingTaskHandle handle;
  object->StartPeriodicTask(&handle, &task_queue);
  // Restart the task
  task_queue.PostTask(TaskHandleStoppper(std::move(handle)));
  object->StartPeriodicTask(&handle, &task_queue);
  task_queue.PostTask(TaskHandleStoppper(std::move(handle)));
  struct Destructor {
    void operator()() { object.reset(); }
    std::unique_ptr<ObjectOnTaskQueue> object;
  };
  task_queue.PostTask(Destructor{std::move(object)});
}
}  // namespace webrtc
