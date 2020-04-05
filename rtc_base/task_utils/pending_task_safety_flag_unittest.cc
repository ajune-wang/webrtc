/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/pending_task_safety_flag.h"

#include "rtc_base/event.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;
}  // namespace

TEST(PendingTaskSafetyFlagTest, Basic) {
  PendingTaskSafetyFlag::Pointer safety_flag;
  {
    // Scope for the |owner| instance.
    class Owner {
     public:
      Owner() = default;
      ~Owner() { flag_->SetNotAlive(); }

      PendingTaskSafetyFlag::Pointer flag_{PendingTaskSafetyFlag::Create()};
    } owner;
    EXPECT_TRUE(owner.flag_->alive());
    safety_flag = owner.flag_;
    EXPECT_TRUE(safety_flag->alive());
  }
  EXPECT_FALSE(safety_flag->alive());
}

}  // namespace webrtc
