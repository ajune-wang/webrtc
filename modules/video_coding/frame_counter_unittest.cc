/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_counter.h"

#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(FrameCounterTest, InitiallyZero) {
  FrameCounter frame_counter;
  EXPECT_EQ(frame_counter.GetUniqueSeen(), 0);
}

TEST(FrameCounterTest, CountsUniqueFrames) {
  FrameCounter frame_counter;

  frame_counter.Add(100);
  EXPECT_EQ(frame_counter.GetUniqueSeen(), 1);
  // Still the same frame.
  frame_counter.Add(100);
  EXPECT_EQ(frame_counter.GetUniqueSeen(), 1);

  // Second frame.
  frame_counter.Add(200);
  EXPECT_EQ(frame_counter.GetUniqueSeen(), 2);
  frame_counter.Add(200);
  EXPECT_EQ(frame_counter.GetUniqueSeen(), 2);

  // Old packet.
  frame_counter.Add(100);
  EXPECT_EQ(frame_counter.GetUniqueSeen(), 2);

  // Missing middle packet.
  frame_counter.Add(150);
  EXPECT_EQ(frame_counter.GetUniqueSeen(), 3);
}

TEST(FrameCounterTest, HasHistoryOfUniqueFrames) {
  const int kNumFrames = 1500;
  const int kRequiredHistoryLength = 1000;
  const uint32_t timestamp = 0xFFFFFFF0;  // Large enough to cause wrap-around.
  FrameCounter frame_counter;

  for (int i = 0; i < kNumFrames; ++i) {
    frame_counter.Add(timestamp + 10 * i);
  }
  ASSERT_EQ(frame_counter.GetUniqueSeen(), kNumFrames);

  // Old packets within history should not affect number of seen unique frames.
  for (int i = kNumFrames - kRequiredHistoryLength; i < kNumFrames; ++i) {
    frame_counter.Add(timestamp + 10 * i);
  }
  ASSERT_EQ(frame_counter.GetUniqueSeen(), kNumFrames);

  // Very old packets should be treated as unique.
  frame_counter.Add(timestamp);
  ASSERT_EQ(frame_counter.GetUniqueSeen(), kNumFrames + 1);
}

}  // namespace
}  // namespace webrtc
