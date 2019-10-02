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

#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {

// TODO(peah): This test is broken in the sense that it not at all tests what it
// seems to test. Enable the test once that is adressed.
TEST(ResidualEchoEstimator, DISABLED_BasicTest) {
  constexpr size_t kNumRenderChannels = 1;
  constexpr size_t kNumCaptureChannels = 1;
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);

  EchoCanceller3Config config;
  config.ep_strength.default_len = 0.f;
  ResidualEchoEstimator estimator(config, kNumRenderChannels);
  AecState aec_state(config, kNumRenderChannels);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, kSampleRateHz, kNumRenderChannels));

  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
  std::array<std::array<float, kFftLengthBy2Plus1>, kNumCaptureChannels>
      S2_linear;
  std::array<float, kFftLengthBy2Plus1> S2_fallback;
  std::array<std::array<float, kFftLengthBy2Plus1>, kNumCaptureChannels> Y2;
  std::array<std::array<float, kFftLengthBy2Plus1>, kNumCaptureChannels> R2;
  EchoPathVariability echo_path_variability(
      false, EchoPathVariability::DelayAdjustment::kNone, false);
  std::vector<std::vector<std::vector<float>>> x(
      kNumBands, std::vector<std::vector<float>>(
                     kNumRenderChannels, std::vector<float>(kBlockSize, 0.f)));
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2(10);
  Random random_generator(42U);
  std::vector<SubtractorOutput> output(kNumRenderChannels);
  std::array<float, kBlockSize> y;
  Aec3Fft fft;
  absl::optional<DelayEstimate> delay_estimate;

  for (auto& H2_k : H2) {
    H2_k.fill(0.01f);
  }
  H2[2].fill(10.f);
  H2[2][0] = 0.1f;

  std::vector<float> h(GetTimeDomainLength(config.filter.main.length_blocks),
                       0.f);

  for (auto& subtractor_output : output) {
    subtractor_output.Reset();
    subtractor_output.s_main.fill(100.f);
  }
  y.fill(0.f);

  constexpr float kLevel = 10.f;
  E2_shadow.fill(kLevel);
  E2_main.fill(kLevel);
  S2_linear[0].fill(kLevel);
  S2_fallback.fill(kLevel);
  Y2[0].fill(kLevel);

  for (int k = 0; k < 1993; ++k) {
    RandomizeSampleVector(&random_generator, x[0][0]);
    std::for_each(x[0][0].begin(), x[0][0].end(), [](float& a) { a /= 30.f; });
    render_delay_buffer->Insert(x);
    if (k == 0) {
      render_delay_buffer->Reset();
    }
    render_delay_buffer->PrepareCaptureProcessing();

    aec_state.HandleEchoPathChange(echo_path_variability);
    aec_state.Update(delay_estimate, H2, h,
                     *render_delay_buffer->GetRenderBuffer(), E2_main, Y2[0],
                     output);

    estimator.Estimate(aec_state, *render_delay_buffer->GetRenderBuffer(),
                       S2_linear, Y2, R2);
  }
  std::for_each(R2[0].begin(), R2[0].end(),
                [&](float a) { EXPECT_NEAR(kLevel, a, 0.1f); });
}

}  // namespace webrtc
