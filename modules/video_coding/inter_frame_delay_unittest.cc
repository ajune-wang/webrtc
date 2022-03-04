/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/inter_frame_delay.h"

#include <limits>

#include "absl/types/optional.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

// Test is for frames at 30fps. At 30fps, RTP timestamps will increase by
// 90000 / 30 = 3000 ticks per frame.
constexpr Frequency k30Fps = Frequency::Hertz(30);
constexpr TimeDelta kFrameDelay = 1 / k30Fps;
constexpr uint32_t kRtpTicksPerFrame = Frequency::KiloHertz(90) / k30Fps;
constexpr Timestamp kStartTime = Timestamp::Millis(1337);

}  // namespace

using ::testing::Eq;
using ::testing::Optional;

TEST(InterFrameDelayTest, OldRtpTimestamp) {
  VCMInterFrameDelay inter_frame_delay;
  EXPECT_THAT(inter_frame_delay.CalculateDelay(180000, kStartTime),
              Optional(TimeDelta::Zero()));
  EXPECT_THAT(inter_frame_delay.CalculateDelay(90000, kStartTime),
              Eq(absl::nullopt));
}

TEST(InterframeDelayTest, NegativeWrapAroundIsSameAsOldRtpTimestamp) {
  VCMInterFrameDelay inter_frame_delay;
  uint32_t rtp = 1500;
  EXPECT_THAT(inter_frame_delay.CalculateDelay(rtp, kStartTime),
              Optional(TimeDelta::Zero()));
  // RTP has wrapped around backwards.
  rtp -= 3000;
  EXPECT_THAT(inter_frame_delay.CalculateDelay(rtp, kStartTime),
              Eq(absl::nullopt));
}

TEST(InterFrameDelayTest, CorrectDelayForFrames) {
  VCMInterFrameDelay inter_frame_delay;
  // Use a fake clock to simplify time keeping.
  SimulatedClock clock(kStartTime);

  // First frame is always delay 0.
  uint32_t rtp = 90000;
  EXPECT_THAT(inter_frame_delay.CalculateDelay(rtp, clock.CurrentTime()),
              Optional(TimeDelta::Zero()));

  // Perfectly timed frame has 0 delay.
  clock.AdvanceTime(kFrameDelay);
  rtp += kRtpTicksPerFrame;
  EXPECT_THAT(inter_frame_delay.CalculateDelay(rtp, clock.CurrentTime()),
              Optional(TimeDelta::Zero()));

  // Slightly early frame will have a negative delay.
  clock.AdvanceTime(kFrameDelay - TimeDelta::Millis(3));
  rtp += kRtpTicksPerFrame;
  EXPECT_THAT(inter_frame_delay.CalculateDelay(rtp, clock.CurrentTime()),
              Optional(-TimeDelta::Millis(3)));

  // Slightly late frame will have positive delay.
  clock.AdvanceTime(kFrameDelay + TimeDelta::Micros(5125));
  rtp += kRtpTicksPerFrame;
  EXPECT_THAT(inter_frame_delay.CalculateDelay(rtp, clock.CurrentTime()),
              Optional(TimeDelta::Micros(5125)));

  // Simulate faster frame RTP at the same clock delay. The frame arrives late,
  // since the RTP timestamp is faster than the delay, and thus is positive.
  clock.AdvanceTime(kFrameDelay);
  rtp += kRtpTicksPerFrame / 2;
  EXPECT_THAT(inter_frame_delay.CalculateDelay(rtp, clock.CurrentTime()),
              Optional(kFrameDelay / 2.0));

  // Simulate slower frame RTP at the same clock delay. The frame is early,
  // since the RTP timestamp advanced more than the delay, and thus is negative.
  clock.AdvanceTime(kFrameDelay);
  rtp += 1.5 * kRtpTicksPerFrame;
  EXPECT_THAT(inter_frame_delay.CalculateDelay(rtp, clock.CurrentTime()),
              Optional(-kFrameDelay / 2.0));
}

TEST(InterFrameDelayTest, PositiveWrapAround) {
  VCMInterFrameDelay inter_frame_delay;
  // Use a fake clock to simplify time keeping.
  SimulatedClock clock(kStartTime);

  // First frame is behind the max RTP by 1500.
  uint32_t rtp = std::numeric_limits<uint32_t>::max() - 1500;
  EXPECT_THAT(inter_frame_delay.CalculateDelay(rtp, clock.CurrentTime()),
              Optional(TimeDelta::Zero()));

  // Rtp wraps around, now 1499.
  rtp += kRtpTicksPerFrame;

  // Frame delay should be as normal, in this case simulated as 1ms late.
  clock.AdvanceTime(kFrameDelay + TimeDelta::Millis(1));
  EXPECT_THAT(inter_frame_delay.CalculateDelay(rtp, clock.CurrentTime()),
              Optional(TimeDelta::Millis(1)));
}

}  // namespace webrtc
