/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/numerics/stats_counter.h"

#include <math.h>
#include <algorithm>
#include <vector>

#include "test/gtest.h"

namespace webrtc {

TEST(BasicStatsCounter, FullTest) {
  std::vector<double> data;
  for (int i = 1; i <= 100; i++) {
    data.push_back(i);
  }
  std::random_shuffle(data.begin(), data.end());

  BasicStatsCounter stats;
  for (double v : data) {
    stats.AddSample(v);
  }

  ASSERT_TRUE(stats.HasValues());
  ASSERT_DOUBLE_EQ(stats.GetMin(), 1.0);
  ASSERT_DOUBLE_EQ(stats.GetMax(), 100.0);
  ASSERT_DOUBLE_EQ(stats.GetAverage(), 50.5);
  for (int i = 1; i <= 100; i++) {
    double p = i / 100.0;
    ASSERT_DOUBLE_EQ(stats.GetPercentile(p), i);
  }
}

}  // namespace webrtc
