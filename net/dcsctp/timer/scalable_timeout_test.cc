/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/timer/scalable_timeout.h"

#include <cstdint>
#include <memory>
#include <string>

#include "net/dcsctp/public/types.h"
#include "rtc_base/gunit.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/time_utils.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::MockFunction;

constexpr TimeoutID kTimeout1Id = TimeoutID(43);
constexpr TimeoutID kTimeout2Id = TimeoutID(44);

constexpr DurationMs kOneRevolution =
    ScalableTimeoutManager::Driver::kBucketCount *
    ScalableTimeoutManager::Driver::kResolution;

class ScalableTimeoutTest : public testing::Test {
 protected:
  ScalableTimeoutTest()
      : driver_([this] { return now_; }),
        factory_(driver_, on_timeout_expired_.AsStdFunction()) {}

  TimeMs now_ = TimeMs(0);
  testing::StrictMock<MockFunction<void(TimeoutID)>> on_timeout_expired_;
  ScalableTimeoutManager::Driver driver_;
  ScalableTimeoutManager factory_;
};

TEST_F(ScalableTimeoutTest, TickWithNoTimers) {
  now_ += kOneRevolution;
  driver_.Tick();
}

TEST_F(ScalableTimeoutTest, StartAndStopTimer) {
  EXPECT_CALL(on_timeout_expired_, Call).Times(0);

  auto timer = factory_.CreateTimeout();

  timer->Start(DurationMs(1000), kTimeout1Id);
  timer->Stop();

  // Ensure it never fires
  now_ += kOneRevolution;
  driver_.Tick();
}

TEST_F(ScalableTimeoutTest, StartAndFireTimer) {
  auto timer = factory_.CreateTimeout();

  timer->Start(DurationMs(100), kTimeout1Id);

  // Should not fire.
  EXPECT_CALL(on_timeout_expired_, Call).Times(0);
  now_ += DurationMs(90);
  driver_.Tick();

  // Should fire.
  EXPECT_CALL(on_timeout_expired_, Call(kTimeout1Id)).Times(1);
  now_ += DurationMs(10);
  driver_.Tick();

  timer->Stop();
}

TEST_F(ScalableTimeoutTest, StopTwoTimers) {
  EXPECT_CALL(on_timeout_expired_, Call).Times(0);

  auto timer1 = factory_.CreateTimeout();
  auto timer2 = factory_.CreateTimeout();

  timer1->Start(DurationMs(100), kTimeout1Id);
  timer2->Start(DurationMs(100), kTimeout2Id);
  timer1->Stop();
  timer2->Stop();

  // Ensure it never fires
  now_ += kOneRevolution;
  driver_.Tick();
}

TimeMs Now() {
  return TimeMs(rtc::TimeMillis());
}

void Run(int num, ScalableTimeoutManager::Driver& driver, TimeMs exit_time) {
  ScalableTimeoutManager factory(driver, [](TimeoutID) {});

  while (Now() < exit_time) {
    auto timer = factory.CreateTimeout();

    for (int i = 0; i < 1000; ++i) {
      timer->Start(DurationMs(100), kTimeout1Id);
      timer->Stop();
    }
    timer->Start(DurationMs(100), kTimeout1Id);
  }
}

void Tick(ScalableTimeoutManager::Driver& driver, TimeMs exit_time) {
  while (Now() < exit_time) {
    driver.Tick();
    rtc::Thread::Current()->SleepMs(
        *ScalableTimeoutManager::Driver::kResolution);
  }
}

struct Actor {
  static void Run(void* obj) {
    auto* me = static_cast<Actor*>(obj);
    me->run();
  }

  std::function<void()> run;
};

TEST_F(ScalableTimeoutTest, MultithreadedTimerTest) {
  ScalableTimeoutManager::Driver driver([]() { return Now(); });

  TimeMs exit_time = TimeMs(rtc::TimeMillis()) + DurationMs(3000);
  Actor a1{[&driver, exit_time] { ::dcsctp::Run(1, driver, exit_time); }};
  Actor a2{[&driver, exit_time] { ::dcsctp::Run(2, driver, exit_time); }};
  Actor a3{[&driver, exit_time] { ::dcsctp::Run(3, driver, exit_time); }};
  Actor atick{[&driver, exit_time] { Tick(driver, exit_time); }};

  rtc::PlatformThread t1(&Actor::Run, &a1, "Actor1");
  rtc::PlatformThread t2(&Actor::Run, &a1, "Actor2");
  rtc::PlatformThread t3(&Actor::Run, &a1, "Actor3");
  rtc::PlatformThread tick(&Actor::Run, &atick, "Ticker");

  t1.Start();
  t2.Start();
  t3.Start();
  tick.Start();

  t1.Stop();
  t2.Stop();
  t3.Stop();
  tick.Stop();
}

}  // namespace
}  // namespace dcsctp
