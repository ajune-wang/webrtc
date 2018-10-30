/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/limiter.h"

#include "absl/memory/memory.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/agc2_testing_common.h"
#include "modules/audio_processing/agc2/vector_float_frame.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

float RunLimiterWithConstantInput(Limiter* limiter,
                                  float input_level,
                                  size_t num_frames,
                                  int sample_rate) {
  // Give time to the level estimator to converge.
  for (size_t i = 0; i < num_frames; ++i) {
    VectorFloatFrame vectors_with_float_frame(
        1, rtc::CheckedDivExact(sample_rate, 100), input_level);
    limiter->Process(vectors_with_float_frame.float_frame_view());
  }

  // Process the last frame with constant input level.
  VectorFloatFrame vectors_with_float_frame_last(
      1, rtc::CheckedDivExact(sample_rate, 100), input_level);
  limiter->Process(vectors_with_float_frame_last.float_frame_view());

  // Return the last sample from the last processed frame.
  const auto channel =
      vectors_with_float_frame_last.float_frame_view().channel(0);
  return channel[channel.size() - 1];
}

}  // namespace

TEST(Limiter, LimiterShouldConstructAndRun) {
  const int sample_rate_hz = 48000;
  ApmDataDumper apm_data_dumper(0);

  Limiter limiter(sample_rate_hz, &apm_data_dumper, "");

  VectorFloatFrame vectors_with_float_frame(1, sample_rate_hz / 100,
                                            kMaxAbsFloatS16Value);
  limiter.Process(vectors_with_float_frame.float_frame_view());
}

TEST(Limiter, OutputVolumeAboveThreshold) {
  const int sample_rate_hz = 48000;
  const float input_level =
      (kMaxAbsFloatS16Value + DbfsToFloatS16(test::kLimiterMaxInputLevelDbFs)) /
      2.f;
  ApmDataDumper apm_data_dumper(0);

  Limiter limiter(sample_rate_hz, &apm_data_dumper, "");

  // Give the level estimator time to adapt.
  for (int i = 0; i < 5; ++i) {
    VectorFloatFrame vectors_with_float_frame(1, sample_rate_hz / 100,
                                              input_level);
    limiter.Process(vectors_with_float_frame.float_frame_view());
  }

  VectorFloatFrame vectors_with_float_frame(1, sample_rate_hz / 100,
                                            input_level);
  limiter.Process(vectors_with_float_frame.float_frame_view());
  rtc::ArrayView<const float> channel =
      vectors_with_float_frame.float_frame_view().channel(0);

  for (const auto& sample : channel) {
    EXPECT_LT(0.9f * kMaxAbsFloatS16Value, sample);
  }
}

TEST(Limiter, RegionHistogramIsUpdated) {
  constexpr size_t kSampleRateHz = 8000;
  constexpr float kInputLevel = 1000.f;
  constexpr size_t kNumFrames = 5;

  metrics::Reset();

  ApmDataDumper apm_data_dumper(0);
  auto limiter =
      absl::make_unique<Limiter>(kSampleRateHz, &apm_data_dumper, "Test");

  static_cast<void>(RunLimiterWithConstantInput(limiter.get(), kInputLevel,
                                                kNumFrames, kSampleRateHz));

  // Destroying Limiter should cause the last limiter region to be logged.
  limiter.reset();

  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Audio.Test.FixedDigitalGainCurveRegion.Identity"));
  EXPECT_EQ(0, metrics::NumSamples(
                   "WebRTC.Audio.Test.FixedDigitalGainCurveRegion.Knee"));
  EXPECT_EQ(0, metrics::NumSamples(
                   "WebRTC.Audio.Test.FixedDigitalGainCurveRegion.Limiter"));
  EXPECT_EQ(0, metrics::NumSamples(
                   "WebRTC.Audio.Test.FixedDigitalGainCurveRegion.Saturation"));
}

}  // namespace webrtc
