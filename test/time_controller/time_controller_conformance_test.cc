/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "rtc_base/event.h"
#include "rtc_base/location.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/real_time_controller.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;
using ::testing::Values;

enum class TimeMode { kRealTime, kSimulated };

std::unique_ptr<TimeController> CreateTimeController(TimeMode mode) {
  switch (mode) {
    case TimeMode::kRealTime:
      return std::make_unique<RealTimeController>();
    case TimeMode::kSimulated:
      // Using an offset of 100000 to get nice fixed width and readable
      // timestamps in typical test scenarios.
      constexpr Timestamp kSimulatedStartTime = Timestamp::Seconds(100000);
      return std::make_unique<GlobalSimulatedTimeController>(
          kSimulatedStartTime);
  }
}

std::string ParamsToString(const TestParamInfo<webrtc::TimeMode>& param) {
  switch (param.param) {
    case webrtc::TimeMode::kRealTime:
      return "RealTime";
    case webrtc::TimeMode::kSimulated:
      return "SimulatedTime";
    default:
      RTC_CHECK(false) << "Time mode not supported";
  }
}

class OrderChecker {
 public:
  void Add(int value) {
    MutexLock lock(&mutex_);
    order_.push_back(value);
  }

  std::vector<int> order() const {
    MutexLock lock(&mutex_);
    return order_;
  }

 private:
  mutable Mutex mutex_;
  std::vector<int> order_ RTC_GUARDED_BY(mutex_);
};

class TimeControllerConformanceTest : public TestWithParam<webrtc::TimeMode> {};

TEST_P(TimeControllerConformanceTest, ThreadPostOrderTest) {
  std::unique_ptr<TimeController> time_controller =
      CreateTimeController(GetParam());
  std::unique_ptr<rtc::Thread> thread = time_controller->CreateThread("thread");

  // Tasks on thread have to be executed in order in which they were
  // posted.
  OrderChecker execution_order;
  thread->PostTask(RTC_FROM_HERE, [&]() { execution_order.Add(1); });
  thread->PostTask(RTC_FROM_HERE, [&]() { execution_order.Add(2); });
  time_controller->AdvanceTime(TimeDelta::Millis(100));
  EXPECT_THAT(execution_order.order(), ElementsAreArray({1, 2}));
}

TEST_P(TimeControllerConformanceTest, ThreadPostDelayedOrderTest) {
  std::unique_ptr<TimeController> time_controller =
      CreateTimeController(GetParam());
  std::unique_ptr<rtc::Thread> thread = time_controller->CreateThread("thread");

  OrderChecker execution_order;
  thread->PostDelayedTask(ToQueuedTask([&]() { execution_order.Add(2); }),
                          /*milliseconds=*/500);
  thread->PostTask(ToQueuedTask([&]() { execution_order.Add(1); }));
  time_controller->AdvanceTime(TimeDelta::Millis(600));
  EXPECT_THAT(execution_order.order(), ElementsAreArray({1, 2}));
}

TEST_P(TimeControllerConformanceTest, ThreadPostInvokeOrderTest) {
  std::unique_ptr<TimeController> time_controller =
      CreateTimeController(GetParam());
  std::unique_ptr<rtc::Thread> thread = time_controller->CreateThread("thread");

  // Tasks on thread have to be executed in order in which they were
  // posted/invoked.
  OrderChecker execution_order;
  thread->PostTask(RTC_FROM_HERE, [&]() { execution_order.Add(1); });
  thread->Invoke<void>(RTC_FROM_HERE, [&]() { execution_order.Add(2); });
  time_controller->AdvanceTime(TimeDelta::Millis(100));
  EXPECT_THAT(execution_order.order(), ElementsAreArray({1, 2}));
}

TEST_P(TimeControllerConformanceTest, ThreadPostInvokeFromThreadOrderTest) {
  std::unique_ptr<TimeController> time_controller =
      CreateTimeController(GetParam());
  std::unique_ptr<rtc::Thread> thread = time_controller->CreateThread("thread");

  // If task is invoked from thread X on thread X it have to be executed
  // immediately.
  OrderChecker execution_order;
  thread->PostTask(RTC_FROM_HERE, [&]() {
    thread->PostTask(RTC_FROM_HERE, [&]() { execution_order.Add(2); });
    thread->Invoke<void>(RTC_FROM_HERE, [&]() { execution_order.Add(1); });
  });
  time_controller->AdvanceTime(TimeDelta::Millis(100));
  EXPECT_THAT(execution_order.order(), ElementsAreArray({1, 2}));
}

TEST_P(TimeControllerConformanceTest, TaskQueuePostEventWaitOrderTest) {
  std::unique_ptr<TimeController> time_controller =
      CreateTimeController(GetParam());
  auto task_queue = time_controller->GetTaskQueueFactory()->CreateTaskQueue(
      "task_queue", webrtc::TaskQueueFactory::Priority::NORMAL);

  // Tasks on thread have to be executed in order in which they were
  // posted/invoked.
  OrderChecker execution_order;
  rtc::Event event;
  task_queue->PostTask(ToQueuedTask([&]() { execution_order.Add(1); }));
  task_queue->PostTask(ToQueuedTask([&]() {
    execution_order.Add(2);
    event.Set();
  }));
  EXPECT_TRUE(event.Wait(/*give_up_after_ms=*/100,
                         /*warn_after_ms=*/10'000));
  time_controller->AdvanceTime(TimeDelta::Millis(100));
  EXPECT_THAT(execution_order.order(), ElementsAreArray({1, 2}));
}

INSTANTIATE_TEST_SUITE_P(ConformanceTest,
                         TimeControllerConformanceTest,
                         Values(TimeMode::kRealTime, TimeMode::kSimulated),
                         ParamsToString);

}  // namespace
}  // namespace webrtc
