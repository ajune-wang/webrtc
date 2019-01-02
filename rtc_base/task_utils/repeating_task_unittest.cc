/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <chrono>
#include <thread>

#include "absl/memory/memory.h"
#include "rtc_base/event.h"
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

constexpr TimeDelta kTimeout = TimeDelta::Millis<1000>();

void Sleep(TimeDelta time_delta) {
  std::this_thread::sleep_for(std::chrono::microseconds(time_delta.us()));
}

class MockClosure {
 public:
  MOCK_METHOD0(Call, TimeDelta());
  MOCK_METHOD0(Delete, void());
};

class MoveOnlyClosure {
 public:
  explicit MoveOnlyClosure(MockClosure* mock) : mock_(mock) {}
  MoveOnlyClosure(const MoveOnlyClosure&) = delete;
  MoveOnlyClosure(MoveOnlyClosure&& other) : mock_(other.mock_) {
    other.mock_ = nullptr;
  }
  ~MoveOnlyClosure() {
    if (mock_)
      mock_->Delete();
  }
  TimeDelta operator()() { return mock_->Call(); }

 private:
  MockClosure* mock_;
};
}  // namespace

TEST(RepeatingTaskTest, TaskIsStoppedOnStop) {
  const TimeDelta kShortInterval = TimeDelta::ms(5);
  const TimeDelta kLongInterval = TimeDelta::ms(20);
  const int kShortIntervalCount = 4;
  const int kMargin = 1;

  rtc::TaskQueue task_queue("TestQueue");
  std::atomic_int counter(0);
  RepeatingTask handle;
  handle.Start(&task_queue, [&] {
    if (++counter >= kShortIntervalCount)
      return kLongInterval;
    return kShortInterval;
  });
  // Sleep long enough to go through the initial phase.
  Sleep(kShortInterval * (kShortIntervalCount + kMargin));
  EXPECT_EQ(counter.load(), kShortIntervalCount);

  handle.PostStop();
  // Sleep long enough that the task would run at least once more if not
  // stopped.
  Sleep(kLongInterval * 2);
  EXPECT_EQ(counter.load(), 4);
}

TEST(RepeatingTaskTest, CompensatesForRunTimeInInclusiveMode) {
  const int kTargetCount = 20;
  const TimeDelta kRepeatInterval = TimeDelta::ms(2);
  const TimeDelta kSleepDuration = TimeDelta::ms(20);
  const int kMargin = 2;
  const int kSleepAtCount = 3;

  rtc::TaskQueue task_queue("TestQueue");
  std::atomic_int counter(0);
  RepeatingTask handle(RepeatingTask::IntervalMode::kIncludingExecution);
  handle.Start(&task_queue, [&] {
    if (++counter == kSleepAtCount)
      Sleep(kSleepDuration);
    return kRepeatInterval;
  });
  Sleep(kRepeatInterval * kTargetCount);
  // Execution time should not have affected the run count,
  // but we allow some margin to reduce flakiness.
  EXPECT_GE(counter.load(), kTargetCount - kMargin);
}

TEST(RepeatingTaskTest, NoCompensationForRunTimeInExclusiveMode) {
  const int kTargetCount = 20;
  const TimeDelta kRepeatInterval = TimeDelta::ms(5);
  const TimeDelta kSleepDuration = TimeDelta::ms(20);
  const int kReducedCount = kTargetCount - kSleepDuration / kRepeatInterval;
  const int kMargin = 2;
  const int kSleepAtCount = 3;

  rtc::TaskQueue task_queue("TestQueue");
  std::atomic_int counter(0);
  RepeatingTask handle(RepeatingTask::IntervalMode::kExcludingExecution);
  handle.Start(&task_queue, [&] {
    if (++counter == kSleepAtCount)
      Sleep(kSleepDuration);
    return kRepeatInterval;
  });
  Sleep(kRepeatInterval * kTargetCount);
  // The longer execution time should have reduced the
  // run count proportionally.
  EXPECT_LE(counter.load(), kReducedCount + kMargin);
}

TEST(RepeatingTaskTest, CancelDelayedTaskBeforeItRuns) {
  rtc::Event done;
  MockClosure mock;
  EXPECT_CALL(mock, Call).Times(0);
  EXPECT_CALL(mock, Delete).WillOnce(Invoke([&done] { done.Set(); }));
  rtc::TaskQueue task_queue("queue");
  RepeatingTask handle;
  handle.DelayStart(&task_queue, TimeDelta::ms(100), MoveOnlyClosure(&mock));
  handle.PostStop();
  EXPECT_TRUE(done.Wait(kTimeout.ms()));
}

TEST(RepeatingTaskTest, CancelTaskAfterItRuns) {
  rtc::Event done;
  MockClosure mock;
  EXPECT_CALL(mock, Call).WillOnce(Return(TimeDelta::ms(100)));
  EXPECT_CALL(mock, Delete).WillOnce(Invoke([&done] { done.Set(); }));
  rtc::TaskQueue task_queue("queue");
  RepeatingTask handle;
  handle.Start(&task_queue, MoveOnlyClosure(&mock));
  handle.PostStop();
  EXPECT_TRUE(done.Wait(kTimeout.ms()));
}

TEST(RepeatingTaskTest, ZeroReturnValueRepostsTheTask) {
  NiceMock<MockClosure> closure;
  rtc::Event done;
  EXPECT_CALL(closure, Call())
      .WillOnce(Return(TimeDelta::Zero()))
      .WillOnce(Invoke([&done] {
        done.Set();
        return kTimeout;
      }));
  rtc::TaskQueue task_queue("queue");
  RepeatingTask().Start(&task_queue, MoveOnlyClosure(&closure));
  EXPECT_TRUE(done.Wait(kTimeout.ms()));
}

TEST(RepeatingTaskTest, StartPeriodicTask) {
  MockFunction<TimeDelta()> closure;
  rtc::Event done;
  EXPECT_CALL(closure, Call())
      .WillOnce(Return(TimeDelta::ms(20)))
      .WillOnce(Return(TimeDelta::ms(20)))
      .WillOnce(Invoke([&done] {
        done.Set();
        return kTimeout;
      }));
  rtc::TaskQueue task_queue("queue");
  RepeatingTask().Start(&task_queue, closure.AsStdFunction());
  EXPECT_TRUE(done.Wait(kTimeout.ms()));
}

// Example to test there are no thread races and use after free for suggested
// typical usage.
TEST(RepeatingTaskTest, Example) {
  class ObjectOnTaskQueue {
   public:
    void DoPeriodicTask() {}
    TimeDelta TimeUntilNextRun() { return TimeDelta::ms(100); }
    void StartPeriodicTask(RepeatingTask* handle, rtc::TaskQueue* task_queue) {
      handle->Start(task_queue, [this] {
        DoPeriodicTask();
        return TimeUntilNextRun();
      });
    }
  };
  rtc::TaskQueue task_queue("queue");
  auto object = absl::make_unique<ObjectOnTaskQueue>();
  // Create and start the periodic task.
  RepeatingTask handle;
  object->StartPeriodicTask(&handle, &task_queue);
  // Restart the task
  handle.PostStop();
  object->StartPeriodicTask(&handle, &task_queue);
  handle.PostStop();
  task_queue.PostTask([&object] { object.reset(); });
  // Do not wait for the destructor closure in order to create a race between
  // task queue destruction and running the desctructor closure.
}

}  // namespace webrtc
