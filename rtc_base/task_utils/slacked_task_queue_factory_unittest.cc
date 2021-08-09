/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/slacked_task_queue_factory.h"

#include <memory>
#include <utility>

#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/task_queue/task_queue_test.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::Invoke;
using ::testing::Le;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::Values;
using ::webrtc::TaskQueueTest;

class MockDelayedCallProvider : public DelayedCallProvider {
 public:
  MOCK_METHOD(void,
              ScheduleDelayedCall,
              (std::unique_ptr<QueuedTask> task, uint32_t milliseconds));
};

class SlackedTaskQueueFactoryTest : public ::testing::Test {
 public:
  SlackedTaskQueueFactoryTest()
      : controller_(Timestamp::Millis(100)),
        mock_delayed_call_provider_temp_(
            std::make_unique<MockDelayedCallProvider>()),
        mock_delayed_call_provider_(mock_delayed_call_provider_temp_.get()),
        factory_(CreateSlackedTaskQueueFactory(
            controller_.GetTaskQueueFactory(),
            std::move(mock_delayed_call_provider_temp_),
            controller_.GetClock())),
        task_queue_(
            factory_->CreateTaskQueue("Test",
                                      TaskQueueFactory::Priority::NORMAL)) {}
  void PostDelayedTask(MockFunction<void(Timestamp)>& fun,
                       uint32_t milliseconds) {
    task_queue_->PostDelayedTask(
        ToQueuedTask([&] { fun.Call(controller_.GetClock()->CurrentTime()); }),
        milliseconds);
  }

 protected:
  GlobalSimulatedTimeController controller_;
  std::unique_ptr<MockDelayedCallProvider> mock_delayed_call_provider_temp_;
  MockDelayedCallProvider* const mock_delayed_call_provider_;
  std::unique_ptr<TaskQueueFactory> factory_;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue_;
};

TEST_F(SlackedTaskQueueFactoryTest, ExecutesImmediateTasksAtDelayedCall) {
  controller_.AdvanceTime(TimeDelta::Millis(1));
  EXPECT_CALL(*mock_delayed_call_provider_, ScheduleDelayedCall)
      .WillOnce(
          Invoke([](std::unique_ptr<QueuedTask> task, uint32_t milliseconds) {
            TaskQueueBase::Current()->PostDelayedTask(std::move(task), 1);
          }));
  MockFunction<void(Timestamp)> function;
  EXPECT_CALL(function, Call(Timestamp::Millis(102))).Times(2);
  PostDelayedTask(function, 0);
  PostDelayedTask(function, 0);
  controller_.AdvanceTime(TimeDelta::Millis(1));
}

TEST_F(SlackedTaskQueueFactoryTest, EventuallyExecutesDelayedCallWithPolls) {
  EXPECT_CALL(*mock_delayed_call_provider_, ScheduleDelayedCall)
      .WillRepeatedly(
          Invoke([](std::unique_ptr<QueuedTask> task, uint32_t milliseconds) {
            TaskQueueBase::Current()->PostDelayedTask(std::move(task), 1);
          }));
  MockFunction<void(Timestamp)> function;
  EXPECT_CALL(function, Call(Timestamp::Millis(150)));
  PostDelayedTask(function, 50);
  controller_.AdvanceTime(TimeDelta::Millis(50));
}

TEST_F(SlackedTaskQueueFactoryTest, ExecutesDelayedTaskAtNextQuantum) {
  MockFunction<void(Timestamp)> function;
  EXPECT_CALL(*mock_delayed_call_provider_, ScheduleDelayedCall)
      .WillOnce(
          Invoke([](std::unique_ptr<QueuedTask> task, uint32_t milliseconds) {
            TaskQueueBase::Current()->PostDelayedTask(std::move(task), 10);
          }));
  EXPECT_CALL(function, Call).Times(0);
  PostDelayedTask(function, 1);
  controller_.AdvanceTime(TimeDelta::Millis(9));
  Mock::VerifyAndClearExpectations(&function);
  EXPECT_CALL(function, Call(Timestamp::Millis(110)));
  controller_.AdvanceTime(TimeDelta::Millis(1));
}

TEST_F(SlackedTaskQueueFactoryTest, ExecutesDelayedTaskAtSequelQuantum) {
  MockFunction<void(Timestamp)> function;
  EXPECT_CALL(*mock_delayed_call_provider_, ScheduleDelayedCall)
      .WillRepeatedly(
          Invoke([](std::unique_ptr<QueuedTask> task, uint32_t milliseconds) {
            TaskQueueBase::Current()->PostDelayedTask(std::move(task), 10);
          }));
  EXPECT_CALL(function, Call).Times(0);
  PostDelayedTask(function, 11);
  controller_.AdvanceTime(TimeDelta::Millis(19));
  Mock::VerifyAndClearExpectations(&function);
  EXPECT_CALL(function, Call(Timestamp::Millis(120)));
  controller_.AdvanceTime(TimeDelta::Millis(1));
}

TEST_F(SlackedTaskQueueFactoryTest, ExecutesDelayedTasksOrderedSequential) {
  MockFunction<void(Timestamp)> function;
  EXPECT_CALL(*mock_delayed_call_provider_, ScheduleDelayedCall)
      .WillRepeatedly(
          Invoke([](std::unique_ptr<QueuedTask> task, uint32_t milliseconds) {
            TaskQueueBase::Current()->PostDelayedTask(
                std::move(task), (milliseconds + 9) / 10 * 10);
          }));
  EXPECT_CALL(function, Call(Timestamp::Millis(110)));
  EXPECT_CALL(function, Call(Timestamp::Millis(120)));
  PostDelayedTask(function, 8);
  PostDelayedTask(function, 17);
  controller_.AdvanceTime(TimeDelta::Millis(20));
}

TEST_F(SlackedTaskQueueFactoryTest, ExecutesDelayedTasksOrderedReverse) {
  MockFunction<void(Timestamp)> function;
  EXPECT_CALL(*mock_delayed_call_provider_, ScheduleDelayedCall)
      .WillRepeatedly(
          Invoke([](std::unique_ptr<QueuedTask> task, uint32_t milliseconds) {
            TaskQueueBase::Current()->PostDelayedTask(
                std::move(task), (milliseconds + 9) / 10 * 10);
          }));
  EXPECT_CALL(function, Call(Timestamp::Millis(110)));
  EXPECT_CALL(function, Call(Timestamp::Millis(120)));
  PostDelayedTask(function, 17);
  PostDelayedTask(function, 8);
  controller_.AdvanceTime(TimeDelta::Millis(20));
}

std::unique_ptr<webrtc::TaskQueueFactory> CreateSlackedQuantumFactory() {
  struct SlackedFactoryWithDefault : public TaskQueueFactory {
    // TaskQueueFactory.
    std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
        absl::string_view name,
        Priority priority) const override {
      return slacked_factory->CreateTaskQueue(name, priority);
    }

    std::unique_ptr<TaskQueueFactory> base_factory{
        CreateDefaultTaskQueueFactory()};
    std::unique_ptr<TaskQueueFactory> slacked_factory{
        CreateSlackedTaskQueueFactory(
            base_factory.get(),
            CreateQuantumDelayedCallProvider(Clock::GetRealTimeClock(),
                                             TimeDelta::Millis(100)),
            Clock::GetRealTimeClock())};
  };
  return std::make_unique<SlackedFactoryWithDefault>();
}

INSTANTIATE_TEST_SUITE_P(
    SlackedTaskQueueFactory,
    TaskQueueTest,
    Values(CreateSlackedQuantumFactory),
    [](const testing::TestParamInfo<TaskQueueTest::ParamType>& info) {
      return "WithQuantumDelayedCallProvider";
    });

}  // namespace
}  // namespace webrtc
