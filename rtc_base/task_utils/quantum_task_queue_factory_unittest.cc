/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/quantum_task_queue_factory.h"

#include <memory>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::Invoke;
using ::testing::Mock;
using ::testing::MockFunction;

class QuantumTaskQueueFactory : public ::testing::Test {
 public:
  QuantumTaskQueueFactory()
      : controller_(Timestamp::Millis(100)),
        factory_(
            CreateQuantumTaskQueueFactory(controller_.GetTaskQueueFactory(),
                                          TimeDelta::Millis(10),
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
  std::unique_ptr<TaskQueueFactory> factory_;
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue_;
};

TEST_F(QuantumTaskQueueFactory, ExecutesImmediateDelayedTaskAtNextQuantum) {
  controller_.AdvanceTime(TimeDelta::Millis(1));

  MockFunction<void(Timestamp)> function;
  EXPECT_CALL(function, Call(Timestamp::Millis(110))).Times(2);
  PostDelayedTask(function, 0);
  controller_.AdvanceTime(TimeDelta::Millis(9));
  PostDelayedTask(function, 0);
  controller_.AdvanceTime(TimeDelta::Millis(0));
}

TEST_F(QuantumTaskQueueFactory, ExecutesDelayedTaskAtNextQuantum) {
  MockFunction<void(Timestamp)> function;
  EXPECT_CALL(function, Call).Times(0);
  PostDelayedTask(function, 1);
  controller_.AdvanceTime(TimeDelta::Millis(9));
  Mock::VerifyAndClearExpectations(&function);
  EXPECT_CALL(function, Call(Timestamp::Millis(110)));
  controller_.AdvanceTime(TimeDelta::Millis(1));
}

TEST_F(QuantumTaskQueueFactory, ExecutesDelayedTaskAtNextNextQuantum) {
  MockFunction<void(Timestamp)> function;
  EXPECT_CALL(function, Call).Times(0);
  PostDelayedTask(function, 11);
  controller_.AdvanceTime(TimeDelta::Millis(19));
  Mock::VerifyAndClearExpectations(&function);
  EXPECT_CALL(function, Call(Timestamp::Millis(120)));
  controller_.AdvanceTime(TimeDelta::Millis(1));
}

}  // namespace
}  // namespace webrtc
