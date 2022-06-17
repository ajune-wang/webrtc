/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/kalman_filter.h"

#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(KalmanFilterTest, AprioriSlopeIs512kbps) {
  KalmanFilter filter;

  EXPECT_EQ(filter.GetSlope(), 1 / (512e3 / 8));
}

TEST(KalmanFilterTest, AposterioSlope) {
  KalmanFilter filter;

  TimeDelta delay_delta1 = TimeDelta::Millis(100);
  double size_delta1 = 1000;
  DataSize max_frame_size = DataSize::Bytes(5000);
  double var_noise = 4.0;
  filter.KalmanEstimateChannel(delay_delta1, size_delta1, max_frame_size,
                               var_noise);

  double delta_bitrate = (size_delta1 * 8.0) / (delay_delta1.ms() / 1000.0);
  printf("delta_bitrate: %f\n", delta_bitrate);

  EXPECT_EQ(filter.GetSlope(), 1.0 / delta_bitrate);
}

}  // namespace
}  // namespace webrtc
