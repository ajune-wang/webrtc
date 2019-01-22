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
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scenario/network/time_controller.h"

namespace webrtc {
namespace test {

TEST(RealTimeControllerTest, StartStop) {
  RealTimeController controller;

  controller.Start();
  rtc::Event e;
  e.Wait(100);
  controller.Stop();

  // Check no crash
}

TEST(RealTimeControllerTest, RunRepeated) {
  RealTimeController controller;

  int run_count = 0;
  controller.RegisterActivity(absl::make_unique<RepeatedActivity2>(
      [&run_count](Timestamp at_time) { ++run_count; }, TimeDelta::ms(100)));

  controller.Start();
  rtc::Event e;
  e.Wait(210);
  controller.Stop();

  // 3 executions at: 0ms, 100ms, 200ms
  ASSERT_EQ(run_count, 3);
}

TEST(RealTimeControllerTest, RunRepeatedWithDelay) {
  RealTimeController controller;

  int run_count = 0;
  controller.RegisterActivity(absl::make_unique<RepeatedActivity2>(
      [&run_count](Timestamp at_time) { ++run_count; }, TimeDelta::ms(100),
      TimeDelta::ms(100)));

  controller.Start();
  rtc::Event e;
  e.Wait(210);
  controller.Stop();

  // 3 executions at: 100ms, 200ms
  ASSERT_EQ(run_count, 2);
}

TEST(RealTimeControllerTest, RunDelayed) {
  RealTimeController controller;

  int run_count = 0;
  Timestamp executed_at = Timestamp::MinusInfinity();
  controller.RegisterActivity(absl::make_unique<DelayedActivity>(
      [&run_count, &executed_at](Timestamp at_time) {
        ++run_count;
        executed_at = at_time;
      },
      TimeDelta::ms(100)));

  controller.Start();
  rtc::Event e;
  e.Wait(210);
  controller.Stop();

  ASSERT_EQ(run_count, 1);
  // ASSERT_EQ
}

TEST(SimulatedTimeControllerTest, StartStop) {
  SimulatedClock sim_clock(Timestamp::seconds(1000).us());
  SimulatedTimeController controller(&sim_clock);

  controller.Start();
  rtc::Event e;
  e.Wait(100);
  controller.Stop();
  // Check that doesn't crash.
}

TEST(SimulatedTimeControllerTest, StartStopRepeated) {
  SimulatedClock sim_clock(Timestamp::seconds(1000).us());
  SimulatedTimeController controller(&sim_clock);

  for (int i = 0; i < 100; ++i) {
    controller.RegisterActivity(absl::make_unique<DelayedActivity>(
        [&controller](Timestamp ignore) { controller.Stop(); },
        TimeDelta::ms(1000)));
    controller.Start();
    controller.AwaitTermination();
  }
}

TEST(SimulatedTimeControllerTest, RepeatedAndDelayedActivityWithTermination) {
  SimulatedClock sim_clock(Timestamp::seconds(1000).us());
  SimulatedTimeController controller(&sim_clock);

  int run_count = 0;
  controller.RegisterActivity(absl::make_unique<RepeatedActivity2>(
      [&run_count](Timestamp at_time) { ++run_count; }, TimeDelta::ms(100)));
  controller.RegisterActivity(absl::make_unique<DelayedActivity>(
      [&controller](Timestamp ignore) { controller.Stop(); },
      TimeDelta::ms(210)));

  controller.Start();
  controller.AwaitTermination();

  // 3 executions at: 0ms, 100ms, 200ms
  ASSERT_EQ(run_count, 3);
}

}  // namespace test
}  // namespace webrtc
