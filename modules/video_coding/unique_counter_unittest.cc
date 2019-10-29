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

TEST(UniqueCounterTest, InitiallyZero) {
  UniqueCounter counter;
  EXPECT_EQ(counter.GetUniqueSeen(), 0);
}

TEST(UniqueCounterTest, CountsUniqueValues) {
  UniqueCounter counter;
  counter.Add(100);
  counter.Add(100);
  counter.Add(200);
  counter.Add(150);
  counter.Add(100);
  EXPECT_EQ(counter.GetUniqueSeen(), 3);
}

TEST(UniqueCounterTest, ForgetsOldValuesAfterTooManyNewValues) {
  const int kNumFrames = UniqueCounter::kMaxHistory + 10;
  const uint32_t timestamp = 0xFFFFFFF0;
  UniqueCounter counter;
  for (int i = 0; i < kNumFrames; ++i) {
    counter.Add(timestamp + 10 * i);
  }
  ASSERT_EQ(counter.GetUniqueSeen(), kNumFrames);
  // slightly old values not affect number of seen unique values.
  for (int i = kNumFrames - UniqueCounter::kMaxHistory; i < kNumFrames; ++i) {
    frame_counter.Add(timestamp + 10 * i);
  }
  EXPECT_EQ(counter.GetUniqueSeen(), kNumFrames);
  // Very old values will be treated as unique.
  frame_counter.Add(timestamp);
  EXPECT_EQ(counter.GetUniqueSeen(), kNumFrames + 1);
}

}  // namespace
}  // namespace webrtc
