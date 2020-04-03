/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_adaptation_counters.h"

#include "test/gtest.h"

namespace webrtc {

TEST(VideoAdaptationCountersTest, Addition) {
  VideoAdaptationCounters a{0, 0};
  VideoAdaptationCounters b{1, 2};
  VideoAdaptationCounters total = a + b;
  EXPECT_EQ(1, total.resolution_adaptations);
  EXPECT_EQ(2, total.fps_adaptations);
}

TEST(VideoAdaptationCountersTest, Subtraction) {
  VideoAdaptationCounters a{0, 1};
  VideoAdaptationCounters b{2, 1};
  VideoAdaptationCounters diff = a - b;
  EXPECT_EQ(-2, diff.resolution_adaptations);
  EXPECT_EQ(0, diff.fps_adaptations);
}

TEST(VideoAdaptationCountersTest, Equality) {
  VideoAdaptationCounters a{1, 2};
  VideoAdaptationCounters b{2, 1};
  EXPECT_EQ(a, a);
  EXPECT_NE(a, b);
}

TEST(VideoAdaptationCountersTest, SelfAdditionSubtraction) {
  VideoAdaptationCounters a{1, 0};
  VideoAdaptationCounters b{0, 1};

  EXPECT_EQ(a, a + b - b);
  EXPECT_EQ(a, b + a - b);
  EXPECT_EQ(a, a - b + b);
  EXPECT_EQ(a, b - b + a);
}

}  // namespace webrtc
