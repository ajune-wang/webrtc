/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/residual_echo_estimator.h"

#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non-null output residual echo power works.
TEST(ResidualEchoEstimator, NullResidualEchoPowerOutput) {
  AecState aec_state(EchoCanceller3Config{});
  FftBuffer fft_buffer(10);
  MatrixBuffer block_buffer(fft_buffer.buffer.size(), 3, kBlockSize);
  VectorBuffer spectrum_buffer(fft_buffer.buffer.size(), kFftLengthBy2Plus1);
  RenderBuffer render_buffer(10, &block_buffer, &spectrum_buffer, &fft_buffer);
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2;
  std::array<float, kFftLengthBy2Plus1> S2_linear;
  std::array<float, kFftLengthBy2Plus1> Y2;
  EXPECT_DEATH(ResidualEchoEstimator(EchoCanceller3Config{})
                   .Estimate(aec_state, render_buffer, S2_linear, Y2, nullptr),
               "");
}

#endif

// TODO(peah): This test is broken in the sense that it not at all tests what it
// seems to test. Enable the test once that is adressed.
TEST(ResidualEchoEstimator, DISABLED_BasicTest) {
  EchoCanceller3Config config;
  config.ep_strength.default_len = 0.f;
  config.delay.min_echo_path_delay_blocks = 0;
  ResidualEchoEstimator estimator(config);
  AecState aec_state(config);
  FftBuffer fft_buffer(GetRenderDelayBufferSize(
      config.delay.down_sampling_factor, config.delay.num_filters));
  MatrixBuffer block_buffer(fft_buffer.buffer.size(), 3, kBlockSize);
  VectorBuffer spectrum_buffer(fft_buffer.buffer.size(), kFftLengthBy2Plus1);
  RenderBuffer render_buffer(10, &block_buffer, &spectrum_buffer, &fft_buffer);
  fft_buffer.Clear();
  block_buffer.Clear();
  spectrum_buffer.Clear();

  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
  std::array<float, kFftLengthBy2Plus1> S2_linear;
  std::array<float, kFftLengthBy2Plus1> S2_fallback;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kFftLengthBy2Plus1> R2;
  EchoPathVariability echo_path_variability(
      false, EchoPathVariability::DelayAdjustment::kNone, false);
  std::vector<std::vector<float>> x(3, std::vector<float>(kBlockSize, 0.f));
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2(10);
  Random random_generator(42U);
  std::array<float, kBlockSize> s;
  Aec3Fft fft;

  for (auto& H2_k : H2) {
    H2_k.fill(0.01f);
  }
  H2[2].fill(10.f);
  H2[2][0] = 0.1f;

  std::array<float, kAdaptiveFilterTimeDomainLength> h;
  h.fill(0.f);

  s.fill(100.f);

  constexpr float kLevel = 10.f;
  E2_shadow.fill(kLevel);
  E2_main.fill(kLevel);
  S2_linear.fill(kLevel);
  S2_fallback.fill(kLevel);
  Y2.fill(kLevel);

  for (int k = 0; k < 1993; ++k) {
    RandomizeSampleVector(&random_generator, x[0]);
    std::for_each(x[0].begin(), x[0].end(), [](float& a) { a /= 30.f; });

    const size_t prev_insert_index = block_buffer.last_insert;

    block_buffer.IncLastInsertIndex();
    spectrum_buffer.DecLastInsertIndex();
    fft_buffer.DecLastInsertIndex();

    for (size_t j = 0; j < x.size(); ++j) {
      std::copy(x[j].begin(), x[j].end(),
                block_buffer.buffer[block_buffer.last_insert][j].begin());
    }
    fft.PaddedFft(block_buffer.buffer[block_buffer.last_insert][0],
                  block_buffer.buffer[prev_insert_index][0],
                  &fft_buffer.buffer[fft_buffer.last_insert]);

    fft_buffer.buffer[fft_buffer.last_insert].Spectrum(
        Aec3Optimization::kNone,
        spectrum_buffer.buffer[spectrum_buffer.last_insert]);

    block_buffer.IncNextReadIndex();
    spectrum_buffer.DecNextReadIndex();
    fft_buffer.DecNextReadIndex();

    render_buffer.UpdateSpectralSum();

    aec_state.HandleEchoPathChange(echo_path_variability);
    aec_state.Update(H2, h, true, 2, render_buffer, E2_main, Y2, x[0], s,
                     false);

    estimator.Estimate(aec_state, render_buffer, S2_linear, Y2, &R2);
  }
  std::for_each(R2.begin(), R2.end(),
                [&](float a) { EXPECT_NEAR(kLevel, a, 0.1f); });
}

}  // namespace webrtc
