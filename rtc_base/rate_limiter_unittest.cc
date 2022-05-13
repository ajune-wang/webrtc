/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/rate_limiter.h"

#include <memory>

#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {

class RateLimitTest : public ::testing::Test {
 public:
  RateLimitTest()
      : clock_(0), rate_limiter(new RateLimiter(&clock_, kWindowSize)) {}
  ~RateLimitTest() override {}

  void SetUp() override { rate_limiter->SetMaxRate(kMaxRate); }

 protected:
  static constexpr TimeDelta kWindowSize = TimeDelta::Seconds(1);
  static constexpr DataRate kMaxRate = DataRate::BitsPerSec(100'000);
  // Bytes needed to completely saturate the rate limiter.
  static constexpr DataSize kRateFilling = kMaxRate * kWindowSize;
  SimulatedClock clock_;
  std::unique_ptr<RateLimiter> rate_limiter;
};

TEST_F(RateLimitTest, IncreasingMaxRate) {
  // Fill rate, extend window to full size.
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFilling / 2));
  clock_.AdvanceTime(kWindowSize - TimeDelta::Millis(1));
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFilling / 2));

  // All rate consumed.
  EXPECT_FALSE(rate_limiter->TryUseRate(DataSize::Bytes(1)));

  // Double the available rate and fill that too.
  rate_limiter->SetMaxRate(kMaxRate * 2);
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFilling));

  // All rate consumed again.
  EXPECT_FALSE(rate_limiter->TryUseRate(DataSize::Bytes(1)));
}

TEST_F(RateLimitTest, DecreasingMaxRate) {
  // Fill rate, extend window to full size.
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFilling / 2));
  clock_.AdvanceTime(kWindowSize - TimeDelta::Millis(1));
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFilling / 2));

  // All rate consumed.
  EXPECT_FALSE(rate_limiter->TryUseRate(DataSize::Bytes(1)));

  // Halve the available rate and move window so half of the data falls out.
  rate_limiter->SetMaxRate(kMaxRate / 2);
  clock_.AdvanceTime(TimeDelta::Millis(1));

  // All rate still consumed.
  EXPECT_FALSE(rate_limiter->TryUseRate(DataSize::Bytes(1)));
}

TEST_F(RateLimitTest, ChangingWindowSize) {
  // Fill rate, extend window to full size.
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFilling / 2));
  clock_.AdvanceTime(kWindowSize - TimeDelta::Millis(1));
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFilling / 2));

  // All rate consumed.
  EXPECT_FALSE(rate_limiter->TryUseRate(DataSize::Bytes(1)));

  // Decrease window size so half of the data falls out.
  rate_limiter->SetWindowSize(kWindowSize / 2);
  // Average rate should still be the same, so rate is still all consumed.
  EXPECT_FALSE(rate_limiter->TryUseRate(DataSize::Bytes(1)));

  // Increase window size again. Now the rate is only half used (removed data
  // points don't come back to life).
  rate_limiter->SetWindowSize(kWindowSize);
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFilling / 2));

  // All rate consumed again.
  EXPECT_FALSE(rate_limiter->TryUseRate(DataSize::Bytes(1)));
}

TEST_F(RateLimitTest, SingleUsageAlwaysOk) {
  // Using more bytes than can fit in a window is OK for a single packet.
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFilling + DataSize::Bytes(1)));
}

TEST_F(RateLimitTest, WindowSizeLimits) {
  EXPECT_TRUE(rate_limiter->SetWindowSize(TimeDelta::Millis(1)));
  EXPECT_FALSE(rate_limiter->SetWindowSize(TimeDelta::Zero()));
  EXPECT_TRUE(rate_limiter->SetWindowSize(kWindowSize));
  EXPECT_FALSE(rate_limiter->SetWindowSize(kWindowSize + TimeDelta::Millis(1)));
}

static const int64_t kMaxTimeoutMs = 30000;

class ThreadTask {
 public:
  explicit ThreadTask(RateLimiter* rate_limiter)
      : rate_limiter_(rate_limiter) {}
  virtual ~ThreadTask() {}

  void Run() {
    start_signal_.Wait(kMaxTimeoutMs);
    DoRun();
    end_signal_.Set();
  }

  virtual void DoRun() = 0;

  RateLimiter* const rate_limiter_;
  rtc::Event start_signal_;
  rtc::Event end_signal_;
};

TEST_F(RateLimitTest, MultiThreadedUsage) {
  // Simple sanity test, with different threads calling the various methods.
  // Runs a few simple tasks, each on its own thread, but coordinated with
  // events so that they run in a serialized order. Intended to catch data
  // races when run with tsan et al.

  // Half window size, double rate -> same amount of bytes needed to fill rate.

  class SetWindowSizeTask : public ThreadTask {
   public:
    explicit SetWindowSizeTask(RateLimiter* rate_limiter)
        : ThreadTask(rate_limiter) {}
    ~SetWindowSizeTask() override {}

    void DoRun() override {
      EXPECT_TRUE(rate_limiter_->SetWindowSize(kWindowSize / 2));
    }
  } set_window_size_task(rate_limiter.get());
  auto thread1 = rtc::PlatformThread::SpawnJoinable(
      [&set_window_size_task] { set_window_size_task.Run(); }, "Thread1");

  class SetMaxRateTask : public ThreadTask {
   public:
    explicit SetMaxRateTask(RateLimiter* rate_limiter)
        : ThreadTask(rate_limiter) {}
    ~SetMaxRateTask() override {}

    void DoRun() override { rate_limiter_->SetMaxRate(kMaxRate * 2); }
  } set_max_rate_task(rate_limiter.get());
  auto thread2 = rtc::PlatformThread::SpawnJoinable(
      [&set_max_rate_task] { set_max_rate_task.Run(); }, "Thread2");

  class UseRateTask : public ThreadTask {
   public:
    UseRateTask(RateLimiter* rate_limiter, SimulatedClock* clock)
        : ThreadTask(rate_limiter), clock_(clock) {}
    ~UseRateTask() override {}

    void DoRun() override {
      EXPECT_TRUE(rate_limiter_->TryUseRate(kRateFilling / 2));
      clock_->AdvanceTime((kWindowSize / 2) - TimeDelta::Millis(1));
      EXPECT_TRUE(rate_limiter_->TryUseRate(kRateFilling / 2));
    }

    SimulatedClock* const clock_;
  } use_rate_task(rate_limiter.get(), &clock_);
  auto thread3 = rtc::PlatformThread::SpawnJoinable(
      [&use_rate_task] { use_rate_task.Run(); }, "Thread3");

  set_window_size_task.start_signal_.Set();
  EXPECT_TRUE(set_window_size_task.end_signal_.Wait(kMaxTimeoutMs));

  set_max_rate_task.start_signal_.Set();
  EXPECT_TRUE(set_max_rate_task.end_signal_.Wait(kMaxTimeoutMs));

  use_rate_task.start_signal_.Set();
  EXPECT_TRUE(use_rate_task.end_signal_.Wait(kMaxTimeoutMs));

  // All rate consumed.
  EXPECT_FALSE(rate_limiter->TryUseRate(DataSize::Bytes(1)));
}

}  // namespace webrtc
