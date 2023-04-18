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

#include <string>

#include "api/units/data_rate.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/metrics.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
// Array of codec types used to run the record metric test.
const VideoCodecType codecs[] = {kVideoCodecVP8, kVideoCodecVP9, kVideoCodecAV1,
                                 kVideoCodecH264};

static std::string CodecTypeToHistogramSuffix(VideoCodecType codec) {
  switch (codec) {
    case kVideoCodecVP8:
      return "Vp8";
    case kVideoCodecVP9:
      return "Vp9";
    case kVideoCodecAV1:
      return "Av1";
    case kVideoCodecH264:
      return "H264";
    case kVideoCodecGeneric:
      return "Generic";
    case kVideoCodecMultiplex:
      return "Multiplex";
  }
}
}  // namespace

class EncoderOvershootDetectorTest : public ::testing::Test {
 public:
  static constexpr int kDefaultBitrateBps = 300000;
  static constexpr double kDefaultFrameRateFps = 15;
  EncoderOvershootDetectorTest()
      : detector_(kWindowSizeMs, kVideoCodecGeneric, false),
        target_bitrate_(DataRate::BitsPerSec(kDefaultBitrateBps)),
        target_framerate_fps_(kDefaultFrameRateFps) {}

 protected:
  void RunConstantUtilizationTest(double actual_utilization_factor,
                                  double expected_utilization_factor,
                                  double allowed_error,
                                  int64_t test_duration_ms) {
    const int frame_size_bytes =
        static_cast<int>(actual_utilization_factor *
                         (target_bitrate_.bps() / target_framerate_fps_) / 8);
    detector_.SetTargetRate(target_bitrate_, target_framerate_fps_,
                            rtc::TimeMillis());

    if (rtc::TimeMillis() == 0) {
      // Encode a first frame which by definition has no overuse factor.
      detector_.OnEncodedFrame(frame_size_bytes, rtc::TimeMillis());
      clock_.AdvanceTime(TimeDelta::Seconds(1) / target_framerate_fps_);
    }

    int64_t runtime_us = 0;
    while (runtime_us < test_duration_ms * 1000) {
      detector_.OnEncodedFrame(frame_size_bytes, rtc::TimeMillis());
      runtime_us += rtc::kNumMicrosecsPerSec / target_framerate_fps_;
      clock_.AdvanceTime(TimeDelta::Seconds(1) / target_framerate_fps_);
    }

    // At constant utilization, both network and media utilization should be
    // close to expected.
    const absl::optional<double> network_utilization_factor =
        detector_.GetNetworkRateUtilizationFactor(rtc::TimeMillis());
    EXPECT_NEAR(network_utilization_factor.value_or(-1),
                expected_utilization_factor, allowed_error);

    const absl::optional<double> media_utilization_factor =
        detector_.GetMediaRateUtilizationFactor(rtc::TimeMillis());
    EXPECT_NEAR(media_utilization_factor.value_or(-1),
                expected_utilization_factor, allowed_error);
  }

  void RunZeroErrorMetricWithNoOvershoot(VideoCodecType codec,
                                         bool is_screenshare) {
    metrics::Reset();
    detector_.SetParametersForUnitTest(codec, is_screenshare);
    DataSize ideal_frame_size =
        target_bitrate_ / Frequency::Hertz(target_framerate_fps_);
    detector_.SetTargetRate(target_bitrate_, target_framerate_fps_,
                            rtc::TimeMillis());
    detector_.OnEncodedFrame(ideal_frame_size.bytes(), rtc::TimeMillis());
    detector_.Reset();
    const std::string kRMSEHistogramPrefix =
        is_screenshare ? "WebRTC.Video.Screenshare.RMSEOfEncodingBitrateInKbps."
                       : "WebRTC.Video.RMSEOfEncodingBitrateInKbps.";
    const std::string kOvershootHistogramPrefix =
        is_screenshare ? "WebRTC.Video.Screenshare.EncodingBitrateOvershoot."
                       : "WebRTC.Video.EncodingBitrateOvershoot.";
    // RMSE and overshoot percent = 0, since we used ideal frame size.
    EXPECT_METRIC_EQ(1, metrics::NumSamples(kRMSEHistogramPrefix +
                                            CodecTypeToHistogramSuffix(codec)));
    EXPECT_METRIC_EQ(
        1, metrics::NumEvents(
               kRMSEHistogramPrefix + CodecTypeToHistogramSuffix(codec), 0));

    EXPECT_METRIC_EQ(1, metrics::NumSamples(kOvershootHistogramPrefix +
                                            CodecTypeToHistogramSuffix(codec)));
    EXPECT_METRIC_EQ(
        1,
        metrics::NumEvents(
            kOvershootHistogramPrefix + CodecTypeToHistogramSuffix(codec), 0));
  }

  void RunMetricWithFiftyPercentOvershoot(VideoCodecType codec,
                                          bool is_screenshare) {
    metrics::Reset();
    detector_.SetParametersForUnitTest(codec, is_screenshare);
    DataSize ideal_frame_size =
        target_bitrate_ / Frequency::Hertz(target_framerate_fps_);
    // Use target frame size with 50% overshoot.
    DataSize target_frame_size = ideal_frame_size * 3 / 2;
    detector_.SetTargetRate(target_bitrate_, target_framerate_fps_,
                            rtc::TimeMillis());
    detector_.OnEncodedFrame(target_frame_size.bytes(), rtc::TimeMillis());
    detector_.Reset();
    const std::string kRMSEHistogramPrefix =
        is_screenshare ? "WebRTC.Video.Screenshare.RMSEOfEncodingBitrateInKbps."
                       : "WebRTC.Video.RMSEOfEncodingBitrateInKbps.";
    const std::string kOvershootHistogramPrefix =
        is_screenshare ? "WebRTC.Video.Screenshare.EncodingBitrateOvershoot."
                       : "WebRTC.Video.EncodingBitrateOvershoot.";
    int64_t rmse_in_kbps = ideal_frame_size.bytes() * 8 / 1000 / 2;
    EXPECT_METRIC_EQ(1, metrics::NumSamples(kRMSEHistogramPrefix +
                                            CodecTypeToHistogramSuffix(codec)));
    EXPECT_METRIC_EQ(
        1, metrics::NumEvents(
               kRMSEHistogramPrefix + CodecTypeToHistogramSuffix(codec),
               rmse_in_kbps));
    // overshoot percent = 50, since we used ideal_frame_size * 3 / 2;
    EXPECT_METRIC_EQ(1, metrics::NumSamples(kOvershootHistogramPrefix +
                                            CodecTypeToHistogramSuffix(codec)));
    EXPECT_METRIC_EQ(
        1,
        metrics::NumEvents(
            kOvershootHistogramPrefix + CodecTypeToHistogramSuffix(codec), 50));
  }

  static constexpr int64_t kWindowSizeMs = 3000;
  EncoderOvershootDetector detector_;
  rtc::ScopedFakeClock clock_;
  DataRate target_bitrate_;
  double target_framerate_fps_;
};

TEST_F(EncoderOvershootDetectorTest, NoUtilizationIfNoRate) {
  const int frame_size_bytes = 1000;
  const int64_t time_interval_ms = 33;
  detector_.SetTargetRate(target_bitrate_, target_framerate_fps_,
                          rtc::TimeMillis());

  // No data points, can't determine overshoot rate.
  EXPECT_FALSE(
      detector_.GetNetworkRateUtilizationFactor(rtc::TimeMillis()).has_value());

  detector_.OnEncodedFrame(frame_size_bytes, rtc::TimeMillis());
  clock_.AdvanceTime(TimeDelta::Millis(time_interval_ms));
  EXPECT_TRUE(
      detector_.GetNetworkRateUtilizationFactor(rtc::TimeMillis()).has_value());
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
  target_bitrate_ = DataRate::BitsPerSec(target_bitrate_.bps() / 2);
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
  detector_.SetTargetRate(target_bitrate_, target_framerate_fps_,
                          rtc::TimeMillis());

  // Test scenario with average bitrate matching the target bitrate, but
  // with some utilization factor penalty as the frames can't be paced out
  // on the network at the target rate.
  // Insert a series of four frames:
  //   1) 20% overshoot, not penalized as buffer if empty.
  //   2) 20% overshoot, the 20% overshoot from the first frame is penalized.
  //   3) 20% undershoot, negating the overshoot from the last frame.
  //   4) 20% undershoot, no penalty.
  // On average then utilization penalty is thus 5%.

  int64_t runtime_us = 0;
  int i = 0;
  while (runtime_us < kWindowSizeMs * rtc::kNumMicrosecsPerMillisec) {
    runtime_us += rtc::kNumMicrosecsPerSec / target_framerate_fps_;
    clock_.AdvanceTime(TimeDelta::Seconds(1) / target_framerate_fps_);
    int frame_size_bytes = (i++ % 4 < 2) ? (ideal_frame_size_bytes * 120) / 100
                                         : (ideal_frame_size_bytes * 80) / 100;
    detector_.OnEncodedFrame(frame_size_bytes, rtc::TimeMillis());
  }

  // Expect 5% overshoot for network rate, see above.
  const absl::optional<double> network_utilization_factor =
      detector_.GetNetworkRateUtilizationFactor(rtc::TimeMillis());
  EXPECT_NEAR(network_utilization_factor.value_or(-1), 1.05, 0.01);

  // Expect media rate to be on average correct.
  const absl::optional<double> media_utilization_factor =
      detector_.GetMediaRateUtilizationFactor(rtc::TimeMillis());
  EXPECT_NEAR(media_utilization_factor.value_or(-1), 1.00, 0.01);
}

TEST_F(EncoderOvershootDetectorTest, RecordsZeroErrorMetricWithNoOvershoot) {
  for (VideoCodecType codec : codecs)
    RunZeroErrorMetricWithNoOvershoot(codec, false);
}

TEST_F(EncoderOvershootDetectorTest, RecordsMetricWithFiftyPercentOvershoot) {
  for (VideoCodecType codec : codecs)
    RunMetricWithFiftyPercentOvershoot(codec, false);
}

TEST_F(EncoderOvershootDetectorTest,
       RecordScreenshareZeroMetricWithNoOvershoot) {
  for (VideoCodecType codec : codecs)
    RunZeroErrorMetricWithNoOvershoot(codec, true);
}

TEST_F(EncoderOvershootDetectorTest,
       RecordScreenshareMetricWithFiftyPercentOvershoot) {
  for (VideoCodecType codec : codecs)
    RunMetricWithFiftyPercentOvershoot(codec, true);
}

}  // namespace webrtc
