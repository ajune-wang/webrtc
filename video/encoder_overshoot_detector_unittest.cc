/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/encoder_overshoot_detector.h"
#include "api/units/data_rate.h"
#include "test/gtest.h"

namespace webrtc {

class EncoderOvershootDetectorTest : public ::testing::Test {
 public:
  EncoderOvershootDetectorTest() : detector_(kWindowSizeMs), now_ms_(1234) {}

 protected:
  static constexpr int64_t kWindowSizeMs = 1000;
  EncoderOvershootDetector detector_;
  int64_t now_ms_;
};

TEST_F(EncoderOvershootDetectorTest, OptimalSize) {
  const DataRate target_bitrate = DataRate::bps(300000);
  const int target_framerate_fps = 30;
  const int frame_size_bytes =
      (target_bitrate.bps() / target_framerate_fps) / 8;
  const int64_t time_interval_ms = 1000 / target_framerate_fps;
  // Allow some error due to inexactness in frame rate measure.
  const double allowed_error = 0.01;

  detector_.SetTargetRate(target_bitrate, target_framerate_fps, now_ms_);

  // No data points, can't determine overshoot rate.
  EXPECT_FALSE(detector_.GetUtilizationFactor(now_ms_));

  detector_.OnEncodedFrame(frame_size_bytes, now_ms_);
  // Single data point is still not enough to be able to estimate rate.
  now_ms_ += time_interval_ms;
  EXPECT_FALSE(detector_.GetUtilizationFactor(now_ms_));

  detector_.OnEncodedFrame(frame_size_bytes, now_ms_);
  now_ms_ += time_interval_ms;
  absl::optional<double> utilization_factor =
      detector_.GetUtilizationFactor(now_ms_);
  EXPECT_NEAR(utilization_factor.value_or(-1), 1.0, allowed_error);

  const int64_t start_ms = now_ms_;
  for (int i = 1; i <= kWindowSizeMs / target_framerate_fps; ++i) {
    now_ms_ = start_ms + ((1000 * i) / target_framerate_fps);
    detector_.OnEncodedFrame(frame_size_bytes, now_ms_);
  }

  utilization_factor = detector_.GetUtilizationFactor(now_ms_);
  EXPECT_NEAR(utilization_factor.value_or(-1), 1.0, allowed_error);
}

}  // namespace webrtc
