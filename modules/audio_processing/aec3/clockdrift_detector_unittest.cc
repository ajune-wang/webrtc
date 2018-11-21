/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/clockdrift_detector.h"

#include "test/gtest.h"

namespace webrtc {
TEST(ClockdriftDetector, ClockdriftDetector) {
  ClockdriftDetector c;
  // No clockdrift at start.
  EXPECT_FALSE(c.ProbableClockdrift());
  EXPECT_FALSE(c.VerifiedClockdrift());

  // Monotonically increasing delay.
  for (int i = 0; i < 100; i++)
    c.Update(1000);
  EXPECT_FALSE(c.ProbableClockdrift());
  EXPECT_FALSE(c.VerifiedClockdrift());
  for (int i = 0; i < 100; i++)
    c.Update(1001);
  EXPECT_FALSE(c.ProbableClockdrift());
  EXPECT_FALSE(c.VerifiedClockdrift());
  for (int i = 0; i < 100; i++)
    c.Update(1002);
  // Probable clockdrift.
  EXPECT_TRUE(c.ProbableClockdrift());
  EXPECT_FALSE(c.VerifiedClockdrift());
  for (int i = 0; i < 100; i++)
    c.Update(1003);
  // Verified clockdrift.
  EXPECT_TRUE(c.ProbableClockdrift());
  EXPECT_TRUE(c.VerifiedClockdrift());

  // Stable delay.
  for (int i = 0; i < 10000; i++)
    c.Update(1003);
  // No clockdrift.
  EXPECT_FALSE(c.ProbableClockdrift());
  EXPECT_FALSE(c.VerifiedClockdrift());

  // Decreasing delay.
  for (int i = 0; i < 100; i++)
    c.Update(1001);
  for (int i = 0; i < 100; i++)
    c.Update(999);
  // Probable clockdrift.
  EXPECT_TRUE(c.ProbableClockdrift());
  EXPECT_FALSE(c.VerifiedClockdrift());
  for (int i = 0; i < 100; i++)
    c.Update(1000);
  for (int i = 0; i < 100; i++)
    c.Update(998);
  // Verified clockdrift.
  EXPECT_TRUE(c.ProbableClockdrift());
  EXPECT_TRUE(c.VerifiedClockdrift());
}
}  // namespace webrtc
