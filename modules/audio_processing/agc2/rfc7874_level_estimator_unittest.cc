/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rfc7874_level_estimator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/agc2_testing_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

constexpr float kMinS16 =
    static_cast<float>(std::numeric_limits<int16_t>::min());
constexpr float kMaxS16 =
    static_cast<float>(std::numeric_limits<int16_t>::max());

Rfc7874AudioLevelEstimator::Levels ComputeAudioLevels(
    rtc::ArrayView<const float> frame) {
  float peak = 0.0f;
  float energy = 0.0f;
  for (const auto& x : frame) {
    peak = std::max(std::fabs(x), peak);
    energy += x * x;
  }
  return {peak, energy};
}

// Helper to create initialized `Rfc7874AudioLevelEstimator` objects.
struct LevelEstimatorHelper {
  explicit LevelEstimatorHelper(int sample_rate_hz)
      : apm_data_dumper(0),
        estimator(
            std::make_unique<Rfc7874AudioLevelEstimator>(sample_rate_hz,
                                                         &apm_data_dumper)) {}
  ApmDataDumper apm_data_dumper;
  std::unique_ptr<Rfc7874AudioLevelEstimator> estimator;
};

// Creates a test signal with the specified sample rate. The signal is the sum
// of white noise and two sinusoidal waves with frequencies below and above
// the cut-off frequency recommended in RFC 7874 sec.4.
std::vector<float> CreateTestSignal(int sample_rate_hz) {
  constexpr float kWhiteNoiseAmplitude = 0.1f;
  test::WhiteNoiseGenerator white_noise_generator(
      /*min_amplitude=*/kWhiteNoiseAmplitude * kMinS16,
      /*max_amplitude=*/kWhiteNoiseAmplitude * kMaxS16);
  constexpr float kSineAmplitude = 0.3f * kMaxS16;
  test::SineGenerator sine0_generator(kSineAmplitude, /*frequency_hz=*/80,
                                      sample_rate_hz);
  test::SineGenerator sine1_generator(kSineAmplitude, /*frequency_hz=*/1000,
                                      sample_rate_hz);

  constexpr int kNum10msFrames = 10;
  const int num_samples = kNum10msFrames * sample_rate_hz / 100;
  std::vector<float> samples(num_samples);
  for (auto& v : samples) {
    v = white_noise_generator() + sine0_generator() + sine1_generator();
  }

  return samples;
}

class Rfc7874AudioLevelEstimatorTest : public ::testing::TestWithParam<int> {
 protected:
  int sample_rate_hz() const { return GetParam(); }
};

// Checks that the audio levels computed by `Rfc7874AudioLevelEstimator` are not
// zero and that they are lower than the corresponding levels computed for the
// original signal.
TEST_P(Rfc7874AudioLevelEstimatorTest, Rfc7874LevelBelowAudioLevel) {
  LevelEstimatorHelper helper(sample_rate_hz());
  const std::vector<float> samples = CreateTestSignal(sample_rate_hz());
  rtc::ArrayView<const float> samples_view(samples);
  const int frame_size = sample_rate_hz() / 100;
  for (size_t i = 0; i < samples.size(); i += frame_size) {
    SCOPED_TRACE(i);
    auto frame = samples_view.subview(i, frame_size);
    const Rfc7874AudioLevelEstimator::Levels levels =
        helper.estimator->GetLevels(frame);
    const Rfc7874AudioLevelEstimator::Levels audio_levels =
        ComputeAudioLevels(frame);
    ASSERT_GT(levels.peak, 0.0f);
    EXPECT_LE(levels.peak, audio_levels.peak);
    ASSERT_GT(levels.energy, 0.0f);
    EXPECT_LE(levels.energy, audio_levels.energy);
  }
}

INSTANTIATE_TEST_SUITE_P(GainController2,
                         Rfc7874AudioLevelEstimatorTest,
                         ::testing::Values(8000, 16000, 32000, 48000));

}  // namespace
}  // namespace webrtc
