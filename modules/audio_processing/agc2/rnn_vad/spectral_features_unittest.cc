/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/spectral_features.h"

#include <algorithm>

#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
#include "rtc_base/checks.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace test {
namespace {

constexpr size_t kTestFeatureVectorSize = kNumBands + 3 * kNumLowerBands + 1;

// Writes non-zero sample values.
void WriteTestData(rtc::ArrayView<float> samples) {
  for (size_t i = 0; i < samples.size(); ++i) {
    samples[i] = i % 100;
  }
}

// Wrapper for SpectralFeaturesExtractor::CheckSilenceComputeFeatures().
bool CheckSilenceComputeFeatures(
    SpectralFeaturesExtractor* sfe,
    rtc::ArrayView<const float, kFrameSize20ms24kHz> samples,
    rtc::ArrayView<float> feature_vector) {
  RTC_DCHECK(sfe);
  return sfe->CheckSilenceComputeFeatures(
      samples, samples,
      {feature_vector.data() + kNumLowerBands, kNumBands - kNumLowerBands},
      {feature_vector.data(), kNumLowerBands},
      {feature_vector.data() + kNumBands, kNumLowerBands},
      {feature_vector.data() + kNumBands + kNumLowerBands, kNumLowerBands},
      {feature_vector.data() + kNumBands + 2 * kNumLowerBands, kNumLowerBands},
      &feature_vector[kNumBands + 3 * kNumLowerBands]);
}

}  // namespace

TEST(RnnVadTest, SpectralFeaturesWithAndWithoutSilence) {
  // Init.
  SpectralFeaturesExtractor sfe;
  std::array<float, kFrameSize20ms24kHz> samples;
  bool is_silence;
  std::array<float, kTestFeatureVectorSize> feature_vector;
  constexpr float kInitialFeatureVal = -9999.f;
  std::fill(feature_vector.begin(), feature_vector.end(), kInitialFeatureVal);

  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;

  // With silence.
  std::fill(samples.begin(), samples.end(), 0.f);
  is_silence = CheckSilenceComputeFeatures(
      &sfe, {samples.data(), samples.size()}, feature_vector);
  // Silence is expected, the output won't be overwritten.
  EXPECT_TRUE(is_silence);
  EXPECT_TRUE(std::all_of(feature_vector.begin(), feature_vector.end(),
                          [](float x) { return x == kInitialFeatureVal; }));

  // With no silence.
  WriteTestData(samples);
  is_silence = CheckSilenceComputeFeatures(
      &sfe, {samples.data(), samples.size()}, feature_vector);
  // Silence is not expected, the output will be overwritten.
  EXPECT_FALSE(is_silence);
  EXPECT_FALSE(std::all_of(feature_vector.begin(), feature_vector.end(),
                           [](float x) { return x == kInitialFeatureVal; }));
}

// When the input signal does not change, the spectral coefficients average does
// not change and the derivatives are zero. Similarly, the spectral variability
// score does not change either.
TEST(RnnVadTest, SpectralFeaturesConstantAverageZeroDerivative) {
  // Init.
  SpectralFeaturesExtractor sfe;
  std::array<float, kFrameSize20ms24kHz> samples;
  WriteTestData(samples);
  bool is_silence;

  std::array<float, kTestFeatureVectorSize> feature_vector;

  // Feed the same frame enough times and then check the output.
  for (size_t i = 0; i < kSpectralCoeffsHistorySize; ++i) {
    is_silence = CheckSilenceComputeFeatures(
        &sfe, {samples.data(), samples.size()}, feature_vector);
  }

  // Feed the same data one last time but on a different output vector.
  std::array<float, kTestFeatureVectorSize> feature_vector_last;
  is_silence = CheckSilenceComputeFeatures(
      &sfe, {samples.data(), samples.size()}, feature_vector_last);

  // Average is unchanged.
  ExpectEqualFloatArray({feature_vector.data(), kNumLowerBands},
                        {feature_vector_last.data(), kNumLowerBands});
  // First and second derivatives are zero.
  constexpr std::array<float, kNumLowerBands> zeros{};
  ExpectEqualFloatArray(
      {feature_vector_last.data() + kNumBands, kNumLowerBands},
      {zeros.data(), zeros.size()});
  ExpectEqualFloatArray(
      {feature_vector_last.data() + kNumBands + kNumLowerBands, kNumLowerBands},
      {zeros.data(), zeros.size()});
  // Spectral variability is unchanged.
  EXPECT_FLOAT_EQ(feature_vector[kNumBands + 3 * kNumLowerBands],
                  feature_vector_last[kNumBands + 3 * kNumLowerBands]);
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
