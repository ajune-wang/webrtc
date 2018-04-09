/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/spectral_features.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

#include "common_audio/resampler/push_sinc_resampler.h"
#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/rnn_vad_fft.h"
#include "common_audio/rnn_vad/sequence_buffer.h"
#include "common_audio/rnn_vad/test_utils.h"
#include "rtc_base/checks.h"
#include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

using rnn_vad::ComputeBandEnergies;
using rnn_vad::ComputeDct;
using rnn_vad::ComputeDctScalingFactor;
using rnn_vad::ComputeDctTable;
using rnn_vad::ComputeLogBandEnergiesCoefficients;
using rnn_vad::ComputeBandBoundaryIndexes;
using rnn_vad::IsSilence;
using rnn_vad::kBufSize24kHz;
using rnn_vad::kFrameSize10ms24kHz;
using rnn_vad::kFrameSize20ms24kHz;
using rnn_vad::kNumBands;
using rnn_vad::kPitchMaxPeriod24kHz;
using rnn_vad::kPitchMaxPeriod48kHz;
using rnn_vad::kSampleRate24kHz;
using rnn_vad::RnnVadFft;
using rnn_vad::SequenceBuffer;
using rnn_vad::SpectralFeaturesExtractor;

// Check that when using precomputed FFT coefficients for frames at 48 kHz, the
// output of ComputeBandEnergies() is bit exact.
TEST(RnnVadTest, ComputeBandEnergies48kHzBitExactness) {
  // Init input data reader and buffers.
  auto fft_coeffs_reader = CreateFftCoeffsReader();
  const size_t num_frames = fft_coeffs_reader.second;
  ASSERT_EQ(
      kFftNumCoeffs20ms48kHz,
      rtc::CheckedDivExact(fft_coeffs_reader.first->data_length(), num_frames) /
          2);
  std::array<float, kFftNumCoeffs20ms48kHz> fft_coeffs_real;
  std::array<float, kFftNumCoeffs20ms48kHz> fft_coeffs_imag;
  std::array<std::complex<float>, kFftNumCoeffs20ms48kHz> fft_coeffs;
  // Init expected output reader and buffer.
  auto band_energies_reader = CreateBandEnergyCoeffsReader();
  ASSERT_EQ(num_frames, band_energies_reader.second);
  std::array<float, kNumBands> expected_band_energies;
  // Init band energies coefficients computation.
  const auto band_boundary_indexes =
      ComputeBandBoundaryIndexes(kSampleRate48kHz, kFrameSize20ms48kHz);
  std::array<float, kNumBands> computed_band_energies;

  // Check output for every frame.
  {
    FloatingPointExceptionObserver fpe_observer;

    for (size_t i = 0; i < num_frames; ++i) {
      SCOPED_TRACE(i);
      // Read input.
      fft_coeffs_reader.first->ReadChunk(
          {fft_coeffs_real.data(), fft_coeffs_real.size()});
      fft_coeffs_reader.first->ReadChunk(
          {fft_coeffs_imag.data(), fft_coeffs_imag.size()});
      for (size_t i = 0; i < kFftNumCoeffs20ms48kHz; ++i) {
        fft_coeffs[i].real(fft_coeffs_real[i]);
        fft_coeffs[i].imag(fft_coeffs_imag[i]);
      }
      band_energies_reader.first->ReadChunk(
          {expected_band_energies.data(), expected_band_energies.size()});
      // Compute band energy coefficients and check output.
      ComputeBandEnergies(
          {fft_coeffs.data(), fft_coeffs.size()},
          {band_boundary_indexes.data(), band_boundary_indexes.size()},
          {computed_band_energies.data(), computed_band_energies.size()});
      ExpectEqualFloatArray(expected_band_energies, computed_band_energies);
    }
  }
}

// This test performs the same verification of
// RnnVad.ComputeBandEnergies48kHzBitExactness() but, instead of using
// pre-computed FFT coefficients, it computes them. Therefore, tolerance on the
// absolute error is allowed.
TEST(RnnVadTest, ComputeFftAndBandEnergies48kHzWithinTolerance) {
  // Preprocessed PCM samples reader and buffers.
  auto samples_reader = CreatePreprocessedSamplesReader();
  const size_t num_frames = samples_reader.second;
  std::array<float, kFrameSize10ms48kHz> samples;
  rtc::ArrayView<float, kFrameSize10ms48kHz> samples_view(samples.data(),
                                                          kFrameSize10ms48kHz);
  // Read ground-truth.
  auto band_energies_reader = CreateBandEnergyCoeffsReader();
  ASSERT_EQ(num_frames, band_energies_reader.second);
  std::array<float, kNumBands> expected_band_energies;
  // Init pipeline.
  SequenceBuffer<float, kFrameSize20ms48kHz, kFrameSize10ms48kHz> seq_buf(0.f);
  auto seq_buf_view = seq_buf.GetBufferView();
  RnnVadFft fft(kFrameSize20ms48kHz);
  std::array<std::complex<float>, kFrameSize20ms48kHz> fft_coeffs;
  const auto band_boundary_indexes =
      ComputeBandBoundaryIndexes(kSampleRate48kHz, kFftLenght20ms48kHz);
  std::array<float, kNumBands> computed_band_energies;

  // Process frames.
  {
    FloatingPointExceptionObserver fpe_observer;

    for (size_t i = 0; i < num_frames; ++i) {
      SCOPED_TRACE(i);
      // Read 10 ms audio frame and corresponding band energies.
      samples_reader.first->ReadChunk(samples_view);
      band_energies_reader.first->ReadChunk(
          {expected_band_energies.data(), expected_band_energies.size()});
      // Run pipeline.
      seq_buf.Push(samples_view);
      fft.ForwardFft(seq_buf_view, {fft_coeffs});
      // Compute band energy coefficients and check output.
      ComputeBandEnergies(
          {fft_coeffs.data(), kFftNumCoeffs20ms48kHz},
          {band_boundary_indexes.data(), band_boundary_indexes.size()},
          {computed_band_energies.data(), computed_band_energies.size()});
      ExpectNearRelative(expected_band_energies, computed_band_energies, 2e-5f);
    }
  }
}

TEST(RnnVadTest, ComputeLogBandEnergiesCoefficientsBitExactness) {
  constexpr std::array<float, kNumBands> input = {
      86.060539245605, 275.668334960938, 43.406528472900, 6.541896820068,
      17.964015960693, 8.090919494629,   1.261920094490,  1.212702631950,
      1.619154453278,  0.508935272694,   0.346316039562,  0.237035423517,
      0.172424271703,  0.271657168865,   0.126088857651,  0.139967113733,
      0.207200810313,  0.155893072486,   0.091090843081,  0.033391401172,
      0.013879744336,  0.011973354965};
  constexpr std::array<float, kNumBands> expected_output = {
      1.934854507446,  2.440402746201,  1.637655138969,  0.816367030144,
      1.254645109177,  0.908534288406,  0.104459829628,  0.087320849299,
      0.211962252855,  -0.284886807203, -0.448164641857, -0.607240796089,
      -0.738917350769, -0.550279200077, -0.866177439690, -0.824003994465,
      -0.663138568401, -0.780171751976, -0.995288193226, -1.362596273422,
      -1.621970295906, -1.658103585243};
  std::array<float, kNumBands> computed_output;
  ComputeLogBandEnergiesCoefficients(
      {input.data(), input.size()},
      {computed_output.data(), computed_output.size()});
  ExpectNearAbsolute({expected_output}, {computed_output}, 1e-5f);
}

TEST(RnnVadTest, ComputeDctBitExactness) {
  constexpr std::array<float, kNumBands> input = {
      0.232155621052,  0.678957760334, 0.220818966627,  -0.077363930643,
      -0.559227049351, 0.432545185089, 0.353900641203,  0.398993015289,
      0.409774333239,  0.454977899790, 0.300520688295,  -0.010286616161,
      0.272525429726,  0.098067551851, 0.083649002016,  0.046226885170,
      -0.033228103071, 0.144773483276, -0.117661058903, -0.005628800020,
      -0.009547689930, -0.045382082462};
  constexpr std::array<float, kNumBands> expected_output = {
      0.697072803974,  0.442710995674,  -0.293156713247, -0.060711503029,
      0.292050391436,  0.489301353693,  0.402255415916,  0.134404733777,
      -0.086305990815, -0.199605688453, -0.234511867166, -0.413774639368,
      -0.388507157564, -0.032798115164, 0.044605545700,  0.112466648221,
      -0.050096966326, 0.045971218497,  -0.029815061018, -0.410366982222,
      -0.209233760834, -0.128037497401};
  const auto dct_table = ComputeDctTable();
  std::array<float, kNumBands> computed_output;
  ComputeDct({input.data(), input.size()}, {dct_table.data(), dct_table.size()},
             ComputeDctScalingFactor(kNumBands),
             {computed_output.data(), computed_output.size()});
  ExpectNearAbsolute({expected_output}, {computed_output}, 1e-5f);
}

TEST(RnnVadTest, ComputeSpectralCoeffcients48kHzWithinTolerance) {
  // Preprocessed PCM samples reader and buffers.
  auto samples_reader = CreatePreprocessedSamplesReader();
  const size_t num_frames = samples_reader.second;
  std::array<float, kFrameSize10ms48kHz> samples;
  rtc::ArrayView<float, kFrameSize10ms48kHz> samples_view(samples.data(),
                                                          kFrameSize10ms48kHz);
  // Read ground-truth.
  auto spectral_coeffs_reader = CreateSpectralCoeffsReader();
  const size_t expected_num_frames_without_silence =
      spectral_coeffs_reader.second;
  ASSERT_LE(expected_num_frames_without_silence, num_frames);
  std::array<float, kNumBands> expected_spectral_coeffs;
  rtc::ArrayView<float, kNumBands> expected_spectral_coeffs_view(
      expected_spectral_coeffs.data(), expected_spectral_coeffs.size());
  // Init pipeline.
  SequenceBuffer<float, kFrameSize20ms48kHz, kFrameSize10ms48kHz> seq_buf(0.f);
  auto reference_frame_view = seq_buf.GetBufferView();
  SpectralFeaturesExtractor<kSampleRate48kHz, kFrameSize20ms48kHz>
      spectral_features_extractor;
  std::array<float, kNumBands> computed_spectral_coeffs;
  // Process frames.
  size_t num_frames_without_silence = 0;
  {
    FloatingPointExceptionObserver fpe_observer;

    for (size_t i = 0; i < num_frames; ++i) {
      SCOPED_TRACE(i);
      // Read 10 ms audio frame.
      samples_reader.first->ReadChunk(samples_view);
      // Run pipeline.
      seq_buf.Push(samples_view);
      // In this test, the feature extractor is only used to compute spectral
      // coefficients; therefore, it is ok to pass the same frame for both
      // reference and lagged frames.
      const bool is_silence = spectral_features_extractor.AnalyzeCheckSilence(
          {reference_frame_view.data(), kFrameSize20ms48kHz},
          {reference_frame_view.data(), kFrameSize20ms48kHz});
      if (is_silence)  // No need to compare when silence is detected.
        continue;
      num_frames_without_silence++;
      // Read expected spectral coefficients.
      spectral_coeffs_reader.first->ReadChunk(
          {expected_spectral_coeffs.data(), expected_spectral_coeffs.size()});
      // Get spectral coefficients and check output.
      spectral_features_extractor.CopySpectralCoefficients(
          {computed_spectral_coeffs});
      ExpectNearRelative({expected_spectral_coeffs}, {computed_spectral_coeffs},
                         6e-4f);
    }
  }
  EXPECT_EQ(expected_num_frames_without_silence, num_frames_without_silence);
}

// TODO(alessiob): Do not include this test since due to the resampling, the
// features change unavoidably.
// Check that the spectral coefficients difference between (i) those computed
// using the porting and (ii) those computed using the reference code is within
// a tolerance. While using the porting, a lower sample rate is used.
TEST(RnnVadTest, DISABLED_ComputeSpectralCoeffcientsWithinTolerance) {
  // Preprocessed PCM samples reader and buffers.
  auto samples_reader = CreatePreprocessedSamplesReader();
  const size_t num_frames = samples_reader.second;
  std::array<float, kFrameSize10ms48kHz> samples_10ms_48kHz;
  rtc::ArrayView<float, kFrameSize10ms48kHz> samples_10ms_48kHz_view(
      samples_10ms_48kHz.data(), samples_10ms_48kHz.size());
  // Read ground-truth.
  auto spectral_coeffs_reader = CreateSpectralCoeffsReader();
  const size_t expected_num_frames_without_silence =
      spectral_coeffs_reader.second;
  ASSERT_LE(expected_num_frames_without_silence, num_frames);
  std::array<float, kNumBands> expected_spectral_coeffs;
  // Init pipeline.
  std::array<float, kFrameSize10ms24kHz> samples_10ms_24kHz;
  rtc::ArrayView<float, kFrameSize10ms24kHz> samples_10ms_24kHz_view(
      samples_10ms_24kHz.data(), samples_10ms_24kHz.size());
  PushSincResampler decimator(kFrameSize10ms48kHz, kFrameSize10ms24kHz);
  SequenceBuffer<float, kBufSize24kHz, kFrameSize10ms24kHz> seq_buf(0.f);
  auto reference_frame_view =
      seq_buf.GetBufferView(kPitchMaxPeriod24kHz, kFrameSize20ms24kHz);
  SpectralFeaturesExtractor<kSampleRate24kHz, kFrameSize20ms24kHz>
      spectral_features_extractor;
  std::array<float, kNumBands> computed_spectral_coeffs;

  FILE* fp_tmp =
      fopen("/usr/local/google/home/alessiob/Desktop/ceps_computed.dat", "wb");

  // Process frames.
  size_t num_frames_without_silence = 0;
  {
    FloatingPointExceptionObserver fpe_observer;

    for (size_t i = 0; i < num_frames; ++i) {
      SCOPED_TRACE(i);
      // Read 10 ms audio frame at 48 kHz, downsample to 24 kHz.
      samples_reader.first->ReadChunk(samples_10ms_48kHz_view);
      decimator.Resample(samples_10ms_48kHz.data(), samples_10ms_48kHz.size(),
                         samples_10ms_24kHz.data(), samples_10ms_24kHz.size());
      // Run pipeline.
      seq_buf.Push(samples_10ms_24kHz_view);
      // In this test, the feature extractor is only used to compute spectral
      // coefficients; therefore, it is ok to pass the same frame for both
      // reference and lagged frames.
      const bool is_silence = spectral_features_extractor.AnalyzeCheckSilence(
          {reference_frame_view.data(), kFrameSize20ms24kHz},
          {reference_frame_view.data(), kFrameSize20ms24kHz});
      if (is_silence)  // No need to compare when silence is detected.
        continue;
      num_frames_without_silence++;
      // Read expected spectral coefficients.
      spectral_coeffs_reader.first->ReadChunk(
          {expected_spectral_coeffs.data(), expected_spectral_coeffs.size()});
      // Get spectral coefficients and check output.
      spectral_features_extractor.CopySpectralCoefficients(
          {computed_spectral_coeffs});
      ExpectNearRelative({expected_spectral_coeffs}, {computed_spectral_coeffs},
                         1e-1f);

      fwrite(computed_spectral_coeffs.data(), sizeof(float),
             computed_spectral_coeffs.size(), fp_tmp);
    }
  }
  EXPECT_EQ(expected_num_frames_without_silence, num_frames_without_silence);

  fclose(fp_tmp);
}

}  // namespace test
}  // namespace webrtc
