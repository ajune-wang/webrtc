/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/frame_delay_delta_kalman_filter.h"

#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(FrameDelayDeltaKalmanFilterTest, InitialBandwidthIs512kbps) {
  FrameDelayDeltaKalmanFilter filter;

  EXPECT_EQ(filter.GetSlope(), 1 / (512e3 / 8));
}

TEST(FrameDelayDeltaKalmanFilterTest,
     ZeroDeltasGiveZeroExpectedAdditionalDelayForNewFilterInstance) {
  // Newly initialized filter, that will not receive any measurement updates
  // in the test.
  FrameDelayDeltaKalmanFilter filter;

  // Assume a frame with zero byte delta, i.e., identical frame size as the
  // previously received frame.
  double zero_size_delta = 0.0;

  // Set the frame delay delta to some value.
  TimeDelta some_delay_delta = TimeDelta::Millis(12);

  // Since the size delta was zero, the newly initialized filter should estimate
  // the delay delta to be identical to the actual measurement value.
  EXPECT_EQ(
      filter.DeviationFromExpectedDelay(some_delay_delta, zero_size_delta),
      some_delay_delta.ms());
}

}  // namespace
}  // namespace webrtc
