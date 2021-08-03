/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/framerate_controller.h"

#include <limits>

#include "rtc_base/time_utils.h"
#include "test/gtest.h"

namespace rtc {
namespace {
constexpr int kCaptureFps = 30;
constexpr int kNumFrames = 60;
}  // namespace

class FramerateControllerTest : public ::testing::Test {
 protected:
  int64_t GetNextTimestampNs() {
    int64_t interval_us = rtc::kNumMicrosecsPerSec / kCaptureFps;
    next_timestamp_us_ += interval_us;
    return next_timestamp_us_ * rtc::kNumNanosecsPerMicrosec;
  }

  int64_t next_timestamp_us_ = rtc::TimeMicros();
  FramerateController controller_;
};

TEST_F(FramerateControllerTest, NoFramesDroppedIfNothingRequested) {
  // Default max framerate is maxint.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_FALSE(controller_.DropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, NoFramesDroppedIfMaxRequested) {
  controller_.SetMaxFramerate(std::numeric_limits<int>::max());

  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_FALSE(controller_.DropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, AllFramesDroppedIfZeroRequested) {
  controller_.SetMaxFramerate(0);

  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_TRUE(controller_.DropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, AllFramesDroppedIfNegativeRequested) {
  controller_.SetMaxFramerate(-1);

  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_TRUE(controller_.DropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, EverySecondFrameDroppedIfHalfRequested) {
  controller_.SetMaxFramerate(kCaptureFps / 2);

  // The first frame should not be dropped.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_EQ(i % 2 == 0, controller_.DropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, EveryThirdFrameDroppedIfTwoThirdsRequested) {
  controller_.SetMaxFramerate(kCaptureFps * 2 / 3);

  // The first frame should not be dropped.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_EQ(i % 3 == 0, controller_.DropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, NoFrameDroppedIfTwiceRequested) {
  controller_.SetMaxFramerate(kCaptureFps * 2);

  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_FALSE(controller_.DropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, NoFrameDroppedForLargeTimestampOffset) {
  controller_.SetMaxFramerate(kCaptureFps);
  EXPECT_FALSE(controller_.DropFrame(0));

  const int64_t kLargeOffsetNs = -987654321LL * 1000;
  EXPECT_FALSE(controller_.DropFrame(kLargeOffsetNs));

  int64_t capture_interval_ns = rtc::kNumNanosecsPerSec / kCaptureFps;
  EXPECT_FALSE(controller_.DropFrame(kLargeOffsetNs + capture_interval_ns));
}

TEST_F(FramerateControllerTest, NoFrameDroppedIfCapturedRequestedWithJitter) {
  controller_.SetMaxFramerate(kCaptureFps);

  // Input capture fps with jitter.
  int64_t capture_interval_ns = rtc::kNumNanosecsPerSec / kCaptureFps;
  EXPECT_FALSE(controller_.DropFrame(capture_interval_ns * 0 / 10));
  EXPECT_FALSE(controller_.DropFrame(capture_interval_ns * 10 / 10 - 1));
  EXPECT_FALSE(controller_.DropFrame(capture_interval_ns * 25 / 10));
  EXPECT_FALSE(controller_.DropFrame(capture_interval_ns * 30 / 10));
  EXPECT_FALSE(controller_.DropFrame(capture_interval_ns * 35 / 10));
  EXPECT_FALSE(controller_.DropFrame(capture_interval_ns * 50 / 10));
}

TEST_F(FramerateControllerTest, FrameDroppedWhenReductionRequested) {
  controller_.SetMaxFramerate(kCaptureFps);

  // Expect no frame drop.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_FALSE(controller_.DropFrame(GetNextTimestampNs()));

  // Reduce max frame rate.
  controller_.SetMaxFramerate(kCaptureFps / 2);

  // Verify that every other frame is dropped.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_EQ(i % 2 == 0, controller_.DropFrame(GetNextTimestampNs()));
}

TEST_F(FramerateControllerTest, NoFramesDroppedAfterReset) {
  controller_.SetMaxFramerate(0);

  // All frames dropped.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_TRUE(controller_.DropFrame(GetNextTimestampNs()));

  controller_.Reset();

  // Expect no frame drop after reset.
  for (int i = 1; i < kNumFrames; ++i)
    EXPECT_FALSE(controller_.DropFrame(GetNextTimestampNs()));
}

}  // namespace rtc
