/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/sample_counter.h"

#include <utility>
#include <vector>

#include "test/gtest.h"

namespace rtc {

TEST(SampleCounterTest, ProcessesNoSamples) {
  const int kMinSamples = 1;
  SampleCounter counter;
  EXPECT_FALSE(counter.Avg(kMinSamples));
  EXPECT_FALSE(counter.Variance(kMinSamples));
  EXPECT_FALSE(counter.Max());
}

TEST(SampleCounterTest, NotEnoughSamples) {
  const int kMinSamples = 6;
  std::vector<int> kTestValues = {1, 2, 3, 4, 5};
  SampleCounter counter;
  for (int value : kTestValues) {
    counter.Add(value);
  }
  EXPECT_FALSE(counter.Avg(kMinSamples));
  EXPECT_FALSE(counter.Variance(kMinSamples));
  EXPECT_TRUE(counter.Max());
}

TEST(SampleCounterTest, EnoughSamples) {
  const int kMinSamples = 5;
  std::vector<int> kTestValues = {1, 2, 3, 4, 5};
  SampleCounter counter;
  for (int value : kTestValues) {
    counter.Add(value);
  }
  EXPECT_TRUE(counter.Avg(kMinSamples));
  EXPECT_EQ(*counter.Avg(kMinSamples), 3);
  EXPECT_TRUE(counter.Variance(kMinSamples));
  EXPECT_EQ(*counter.Variance(kMinSamples), 2);
  EXPECT_TRUE(counter.Max());
  EXPECT_EQ(*counter.Max(), 5);
}

}  // namespace rtc
