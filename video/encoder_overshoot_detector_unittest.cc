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
  static constexpr int kDefaultBitrateBps = 300000;
  static constexpr int kDefaultFrameRateFps = 30;
  EncoderOvershootDetectorTest()
      : detector_(kWindowSizeMs),
        now_ms_(1234),
        target_bitrate_(DataRate::bps(kDefaultBitrateBps)),
        target_framerate_fps_(kDefaultFrameRateFps) {}

 protected:
  void RunConstantUtilizationTest(double actual_utilization_factor,
                                  double expected_utilization_factor,
                                  double allowed_error,
                                  int64_t test_duration_ms) {
    const int frame_size_bytes =
        static_cast<int>(actual_utilization_factor *
                         (target_bitrate_.bps() / target_framerate_fps_) / 8);
    detector_.SetTargetRate(target_bitrate_, target_framerate_fps_, now_ms_);

    const int64_t start_ms = now_ms_;
    for (int i = 1; i <= test_duration_ms / target_framerate_fps_; ++i) {
      now_ms_ = start_ms + ((1000 * i) / target_framerate_fps_);
      detector_.OnEncodedFrame(frame_size_bytes, now_ms_);
    }

    absl::optional<double> utilization_factor =
        detector_.GetUtilizationFactor(now_ms_);
    EXPECT_NEAR(utilization_factor.value_or(-1), expected_utilization_factor,
                allowed_error);
  }

  static constexpr int64_t kWindowSizeMs = 1000;
  EncoderOvershootDetector detector_;
  int64_t now_ms_;
  DataRate target_bitrate_;
  int target_framerate_fps_;
};

TEST_F(EncoderOvershootDetectorTest, NoUtililizationIfNoRate) {
  const int frame_size_bytes = 1000;
  const int64_t time_interval_ms = 33;

  // No data points, can't determine overshoot rate.
  EXPECT_FALSE(detector_.GetUtilizationFactor(now_ms_));

  detector_.OnEncodedFrame(frame_size_bytes, now_ms_);
  now_ms_ += time_interval_ms;
  EXPECT_TRUE(detector_.GetUtilizationFactor(now_ms_));
}

TEST_F(EncoderOvershootDetectorTest, OptimalSize) {
  // Optimally behaved encoder.
  // Allow some error margin due to rounding errors, eg due to frame
  // interval not being an integer.
  RunConstantUtilizationTest(1.0, 1.0, 0.01, kWindowSizeMs);
}

TEST_F(EncoderOvershootDetectorTest, Undershoot) {
  // Undershoot, reported utilization factor should be capped to 1.0 so
  // that we don't incorrectly boost encoder bitrate during movement.
  RunConstantUtilizationTest(0.5, 1.0, 0.00, kWindowSizeMs);
}

TEST_F(EncoderOvershootDetectorTest, Overshoot) {
  // Overshoot by 20%.
  // Allow some error margin due to rounding errors.
  RunConstantUtilizationTest(1.2, 1.2, 0.01, kWindowSizeMs);
}

TEST_F(EncoderOvershootDetectorTest, ConstantOvershootVaryingRates) {
  // Overshoot by 20%, but vary framerate and bitrate.
  // Allow some error margin due to rounding errors.
  RunConstantUtilizationTest(1.2, 1.2, 0.01, kWindowSizeMs);
  target_framerate_fps_ /= 2;
  RunConstantUtilizationTest(1.2, 1.2, 0.01, kWindowSizeMs / 2);
  target_bitrate_ = DataRate::bps(target_bitrate_.bps() * 2);
  RunConstantUtilizationTest(1.2, 1.2, 0.01, kWindowSizeMs / 2);
}

TEST_F(EncoderOvershootDetectorTest, ConstantRateVaryingOvershoot) {
  // Overshoot by 10%, keep framerate and bitrate constant.
  // Allow some error margin due to rounding errors.
  RunConstantUtilizationTest(1.1, 1.1, 0.01, kWindowSizeMs);

  // Change overshoot to 20%, run for half window and expect overshoot
  // to be 15%.
  RunConstantUtilizationTest(1.2, 1.15, 0.01, kWindowSizeMs / 2);

  // Keep running at 20% overshoot, after window is full that should now
  // be the reported overshoot.
  RunConstantUtilizationTest(1.2, 1.2, 0.01, kWindowSizeMs / 2);
}

TEST_F(EncoderOvershootDetectorTest, PartialOvershoot) {
  const int ideal_frame_size_bytes =
      (target_bitrate_.bps() / target_framerate_fps_) / 8;
  detector_.SetTargetRate(target_bitrate_, target_framerate_fps_, now_ms_);

  // Test scenario with average bitrate matching the target bitrate, but
  // with some utilization factor penalty as the frames can't be paced out
  // on the network at the target rate.
  // Insert a series of four frames:
  //   1) 20% overshoot, not penalized as buffer if empty.
  //   2) 20% overshoot, the 20% overshoot from the first frame is penalized.
  //   3) 20% undershoot, negating the overshoot from the last frame.
  //   4) 20% undershoot, no penalty.
  // On average then utilization penalty is thus 5%.

  const int64_t start_ms = now_ms_;
  for (int i = 0; i <= kWindowSizeMs / target_framerate_fps_; ++i) {
    now_ms_ = start_ms + ((1000 * i) / target_framerate_fps_);
    int frame_size_bytes = (i % 4 < 2) ? (ideal_frame_size_bytes * 120) / 100
                                       : (ideal_frame_size_bytes * 80) / 100;
    detector_.OnEncodedFrame(frame_size_bytes, now_ms_);
  }

  absl::optional<double> utilization_factor =
      detector_.GetUtilizationFactor(now_ms_);
  EXPECT_NEAR(utilization_factor.value_or(-1), 1.05, 0.01);
}

}  // namespace webrtc
