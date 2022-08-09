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

// This test verifies that the initial filter state (link bandwidth, link
// propagation delay) is such that a frame of size zero would take no time to
// propagate.
TEST(FrameDelayDeltaKalmanFilterTest,
     InitialStateFilterWithZeroSizeFrameTakesNoTimeToPropagate) {
  FrameDelayDeltaKalmanFilter filter;

  // A zero-sized frame...
  double frame_size_delta_bytes = 0.0;

  // ...should take no time to propagate due to it's size...
  EXPECT_EQ(filter.GetFrameDelayDeltaEstimateSizeBased(frame_size_delta_bytes),
            0.0);

  // ...and no time due to the initial link propagation delay being zero.
  EXPECT_EQ(filter.GetFrameDelayDeltaEstimateTotal(frame_size_delta_bytes),
            0.0);
}

TEST(FrameDelayDeltaKalmanFilterTest,
     InitialStateFilterWithSmallSizeFrameTakesFixedTimeToPropagate) {
  FrameDelayDeltaKalmanFilter filter;

  // A small-sized frame...
  double frame_size_delta_bytes = 1.0;
  double expected_frame_delay_delta_estimate = 1.0 / (512e3 / 8.0);
  EXPECT_EQ(filter.GetFrameDelayDeltaEstimateSizeBased(frame_size_delta_bytes),
            expected_frame_delay_delta_estimate);
  EXPECT_EQ(filter.GetFrameDelayDeltaEstimateTotal(frame_size_delta_bytes),
            expected_frame_delay_delta_estimate);
}

}  // namespace
}  // namespace webrtc
