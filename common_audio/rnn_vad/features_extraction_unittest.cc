/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/downsample.h"
#include "common_audio/rnn_vad/features_extraction.h"
#include "common_audio/rnn_vad/rnn_vad_fft.h"
#include "common_audio/rnn_vad/sequence_buffer.h"
#include "common_audio/rnn_vad/test_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

using rnn_vad::BiQuadFilter;
using rnn_vad::ComputeBandEnergies;
using rnn_vad::ComputeOpusBandBoundaries;
using rnn_vad::Decimate48k24k;
using rnn_vad::k48kHpfConfig;
using rnn_vad::k10msFrameSize;
using rnn_vad::k20msFrameSize;
using rnn_vad::kNumOpusBands;
using rnn_vad::kSampleRate;
using rnn_vad::RnnVadFeaturesExtractor;
using rnn_vad::RnnVadFft;
using rnn_vad::SequenceBuffer;

namespace {

void CheckBandEnergies(rtc::ArrayView<const float> expected_coeffs,
                       rtc::ArrayView<const float> computed_coeffs,
                       const float tot_energy_tolerance,
                       const float band_log_energy_tolerance) {
  // Helpers.
  auto tot_energy = [](rtc::ArrayView<const float> v) {
    return std::accumulate(v.begin(), v.end(), 0.f);
  };
  auto relative_err = [](float e, float c) {
    return (e == 0.f) ? 0.f : std::fabs(e - c) / e;
  };
  // Check total energy.
  const float expected_tot_energy = tot_energy(expected_coeffs);
  const float computed_tot_energy = tot_energy(computed_coeffs);
  ASSERT_LE(0.f, expected_tot_energy);
  EXPECT_LE(0.f, computed_tot_energy);
  EXPECT_LE(relative_err(expected_tot_energy, computed_tot_energy),
            tot_energy_tolerance);
  // Check absolute error for the log of the band energy coefficients.
  for (size_t i = 0; i < expected_coeffs.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_LE(0.f, computed_coeffs[i]);
    if (expected_coeffs[i] > 0.f && computed_coeffs[i] > i)
      EXPECT_NEAR(std::log(expected_coeffs[i]), std::log(computed_coeffs[i]),
                  band_log_energy_tolerance);
  }
}

}  // namespace

// Compare ComputeBandEnergies against the reference code.
TEST(RnnVad, ComputeBandEnergiesBitExactness) {
  constexpr size_t num_fft_points = k48k20msFrameSize / 2 + 1;
  // Init input data reader and buffers.
  auto fft_coeffs_reader = CreateFftCoeffsReader();
  const size_t num_frames = fft_coeffs_reader.second;
  ASSERT_EQ(
      num_fft_points,
      rtc::CheckedDivExact(fft_coeffs_reader.first->data_length(), num_frames) /
          2);
  std::array<float, num_fft_points> fft_coeffs_real;
  std::array<float, num_fft_points> fft_coeffs_imag;
  AlignedArray<std::complex<float>> fft_coeffs(
      1, num_fft_points, RealFourier::kFftBufferAlignment);
  // Init expected output reader and buffer.
  auto band_energies_reader = CreateBandEnergyCoeffsReader();
  ASSERT_EQ(num_frames, band_energies_reader.second);
  std::array<float, kNumOpusBands> expected_band_energies;
  // Init band energies coefficients computation.
  const auto band_boundary_indexes =
      ComputeOpusBandBoundaries(kSampleRate48k, k48k20msFrameSize);
  std::array<float, kNumOpusBands> computed_band_energies;

  // The read FFT coefficients must be adapted to the expected FFT scaling
  // scheme.
  const float fft_coeffs_scaling = static_cast<float>(k48k20msFrameSize);

  // Check output for every frame.
  for (size_t i = 0; i < num_frames; ++i) {
    SCOPED_TRACE(i);
    // Read input.
    fft_coeffs_reader.first->ReadChunk(
        {fft_coeffs_real.data(), fft_coeffs_real.size()});
    fft_coeffs_reader.first->ReadChunk(
        {fft_coeffs_imag.data(), fft_coeffs_imag.size()});
    for (size_t i = 0; i < num_fft_points; ++i) {
      fft_coeffs.Row(0)[i] =
          fft_coeffs_scaling *
          std::complex<float>(fft_coeffs_real[i], fft_coeffs_imag[i]);
    }
    band_energies_reader.first->ReadChunk(
        {expected_band_energies.data(), expected_band_energies.size()});
    // Compute band energy coefficients and check output.
    ComputeBandEnergies({fft_coeffs.Row(0), num_fft_points},
                        {band_boundary_indexes}, {computed_band_energies});
    ExpectNearRelative(expected_band_energies, computed_band_energies, 1e-6f);
  }
}

TEST(RnnVad, ComputeBandCorrelationsBitExactness) {
  // TODO(alessiob): Implement.
}

TEST(RnnVad, ComputeDctBitExactness) {
  // TODO(alessiob): Implement.
}

TEST(RnnVad, ComputeSpectralVariabilityBitExactness) {
  // TODO(alessiob): Implement.
}

// Check that the band energy coefficients difference between (i) those computed
// using the porting and (ii) those computed using the reference code is within
// a tolerance. While using the porting, the same sample rate of the reference
// code is used; therefore, the only difference is caused by the FFT size.
TEST(RnnVad, CheckComputeBandEnergiesPipeline) {
  // PCM samples reader and buffers.
  auto samples_reader = CreatePcmSamplesReader(k48k10msFrameSize);
  const size_t num_frames = samples_reader.second;
  std::array<float, k48k10msFrameSize> samples;
  rtc::ArrayView<float, k48k10msFrameSize> samples_view(samples.data(),
                                                        k48k10msFrameSize);
  // Read ground-truth.
  auto band_energies_reader = CreateBandEnergyCoeffsReader();
  ASSERT_EQ(num_frames, band_energies_reader.second);
  std::array<float, kNumOpusBands> expected_band_energies;
  // Init pipeline.
  SequenceBuffer<float, k48k20msFrameSize, k48k10msFrameSize> seq_buf(0.f);
  auto seq_buf_view = seq_buf.GetBufferView();
  BiQuadFilter hpf(k48kHpfConfig);
  RnnVadFft fft(k48k20msFrameSize);
  auto fft_output_buf_view = fft.GetFftOutputBufferView();
  const auto band_boundary_indexes =
      ComputeOpusBandBoundaries(kSampleRate48k, k48k20msFftSize);
  std::array<float, kNumOpusBands> computed_band_energies;

  // Process frames.
  for (size_t i = 0; i < num_frames; ++i) {
    SCOPED_TRACE(i);
    // Read 10 ms audio frame and corresponding band energies.
    samples_reader.first->ReadChunk(samples_view);
    band_energies_reader.first->ReadChunk(
        {expected_band_energies.data(), expected_band_energies.size()});
    // Run pipeline and check output.
    hpf.ProcessFrame(samples_view, samples_view);
    seq_buf.Push(samples_view);
    fft.ForwardFft(seq_buf_view);
    ComputeBandEnergies(fft_output_buf_view, {band_boundary_indexes},
                        {computed_band_energies});
    CheckBandEnergies({expected_band_energies}, {computed_band_energies}, 0.1f,
                      1.5f);
  }
}

// Check that the band energy coefficients difference between (i) those computed
// using the porting and (ii) those computed using the reference code is within
// a tolerance. While using the porting, a reduced sample rate is used.
TEST(RnnVad, CheckComputeBandEnergiesPipelineLowerSampleRate) {
  // PCM samples reader and buffers.
  auto samples_reader = CreatePcmSamplesReader(k48k10msFrameSize);
  const size_t num_frames = samples_reader.second;
  std::array<float, k48k10msFrameSize> samples_48k_10ms;
  // Read ground-truth.
  auto band_energies_reader = CreateBandEnergyCoeffsReader();
  ASSERT_EQ(num_frames, band_energies_reader.second);
  std::array<float, kNumOpusBands> expected_band_energies;
  // Init pipeline.
  ASSERT_EQ(k48k10msFrameSize / 2, k10msFrameSize);
  ASSERT_EQ(k48k20msFrameSize / 2, k20msFrameSize);
  std::array<float, k10msFrameSize> samples_10ms;
  rtc::ArrayView<float, k10msFrameSize> samples_10ms_view(samples_10ms.data(),
                                                          samples_10ms.size());
  SequenceBuffer<float, k20msFrameSize, k10msFrameSize> seq_buf(0.f);
  auto seq_buf_view = seq_buf.GetBufferView();
  BiQuadFilter hpf(
      {-1.98889291, 0.98895425, 0.99446179, -1.98892358, 0.99446179});
  RnnVadFft fft(k20msFrameSize);
  auto fft_output_buf_view = fft.GetFftOutputBufferView();
  const auto band_boundary_indexes =
      ComputeOpusBandBoundaries(kSampleRate, k20msFrameSize);
  std::array<float, kNumOpusBands> computed_band_energies;

  // Process frames.
  for (size_t i = 0; i < num_frames; ++i) {
    SCOPED_TRACE(i);
    // Read 10 ms audio frame and corresponding band energies.
    samples_reader.first->ReadChunk({samples_48k_10ms});
    band_energies_reader.first->ReadChunk(
        {expected_band_energies.data(), expected_band_energies.size()});
    // Run pipeline and check output.
    Decimate48k24k({samples_48k_10ms}, samples_10ms_view);
    hpf.ProcessFrame(samples_10ms_view, samples_10ms_view);
    seq_buf.Push(samples_10ms_view);
    fft.ForwardFft(seq_buf_view);
    ComputeBandEnergies(fft_output_buf_view, {band_boundary_indexes},
                        {computed_band_energies});
    CheckBandEnergies({expected_band_energies}, {computed_band_energies}, 0.76f,
                      4.5f);
  }
}

// Check that the RNN VAD features difference between (i) those computed
// using the porting and (ii) those computed using the reference code is within
// a tolerance.
TEST(RnnVad, CheckExtractedFeaturesAreNear) {
  // TODO(alessiob): Implement.
}

}  // namespace test
}  // namespace webrtc
