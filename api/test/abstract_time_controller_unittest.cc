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

#include <atomic>
#include <memory>
#include <utility>

#include "rtc_base/event.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "test/gmock.h"
#include "test/gtest.h"

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

class FakeTimeController : public AbstractTimeController {
 public:
  explicit FakeTimeController(SimulatedClock* clock);

  void ScheduleAt(Timestamp time) override;
  void RunFor(TimeDelta duration) override;

 private:
  SimulatedClock* clock_;
  Timestamp next_run_time_;
};

FakeTimeController::FakeTimeController(SimulatedClock* clock)
    : AbstractTimeController(clock),
      clock_(clock),
      next_run_time_(Timestamp::PlusInfinity()) {}

void FakeTimeController::ScheduleAt(Timestamp time) {
  if (time < next_run_time_) {
    next_run_time_ = time;
  }
}

void FakeTimeController::RunFor(TimeDelta duration) {
  Timestamp end_time = clock_->CurrentTime() + duration;

  while (next_run_time_ <= end_time) {
    clock_->AdvanceTime(next_run_time_ - clock_->CurrentTime());
    next_run_time_ = Timestamp::PlusInfinity();
    Run();
  }

  clock_->AdvanceTime(end_time - clock_->CurrentTime());
}

}  // namespace

TEST(AbstractTimeControllerTest, TaskIsStoppedOnStop) {
  const TimeDelta kShortInterval = TimeDelta::ms(5);
  const TimeDelta kLongInterval = TimeDelta::ms(20);
  const int kShortIntervalCount = 4;
  const int kMargin = 1;
  SimulatedClock clock(kStartTime);
  FakeTimeController time_simulation(&clock);
  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));
  std::atomic_int counter(0);
  auto handle = RepeatingTaskHandle::Start(task_queue.Get(), [&] {
    if (++counter >= kShortIntervalCount)
      return kLongInterval;
    return kShortInterval;
  });
  // Sleep long enough to go through the initial phase.
  time_simulation.Sleep(kShortInterval * (kShortIntervalCount + kMargin));
  EXPECT_EQ(counter.load(), kShortIntervalCount);

  task_queue.PostTask(
      [handle = std::move(handle)]() mutable { handle.Stop(); });

  // Sleep long enough that the task would run at least once more if not
  // stopped.
  time_simulation.Sleep(kLongInterval * 2);
  EXPECT_EQ(counter.load(), kShortIntervalCount);
}

TEST(AbstractTimeControllerTest, TaskCanStopItself) {
  std::atomic_int counter(0);
  SimulatedClock clock(kStartTime);
  FakeTimeController time_simulation(&clock);
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
  time_simulation.Sleep(TimeDelta::ms(10));
  EXPECT_EQ(counter.load(), 1);
}

TEST(AbstractTimeControllerTest, YieldForTask) {
  SimulatedClock clock(kStartTime);
  FakeTimeController time_simulation(&clock);

  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));

  time_simulation.InvokeWithControlledYield([&] {
    rtc::Event event;
    task_queue.PostTask([&] { event.Set(); });
    EXPECT_TRUE(event.Wait(200));
  });
}

TEST(AbstractTimeControllerTest, TasksYieldToEachOther) {
  SimulatedClock clock(kStartTime);
  FakeTimeController time_simulation(&clock);

  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));
  rtc::TaskQueue other_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "OtherQueue", TaskQueueFactory::Priority::NORMAL));

  task_queue.PostTask([&] {
    rtc::Event event;
    other_queue.PostTask([&] { event.Set(); });
    EXPECT_TRUE(event.Wait(200));
  });

  time_simulation.Sleep(TimeDelta::ms(300));
}

TEST(AbstractTimeControllerTest, CurrentTaskQueue) {
  SimulatedClock clock(kStartTime);
  FakeTimeController time_simulation(&clock);

  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));

  task_queue.PostTask([&] { EXPECT_TRUE(task_queue.IsCurrent()); });

  time_simulation.Sleep(TimeDelta::ms(10));
}

}  // namespace webrtc
