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
#include <cstdio>  // TODO(alessiob): Remove.
#include <numeric>

#include "common_audio/resampler/push_sinc_resampler.h"
#include "common_audio/rnn_vad/biquad.h"
#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/rnn_vad_fft.h"
#include "common_audio/rnn_vad/sequence_buffer.h"
#include "common_audio/rnn_vad/test_utils.h"
#include "rtc_base/checks.h"
#include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

using rnn_vad::BiQuadFilter;
using rnn_vad::ComputeBandEnergies;
using rnn_vad::ComputeDct;
using rnn_vad::ComputeDctScalingFactor;
using rnn_vad::ComputeDctTable;
using rnn_vad::ComputeLogBandEnergiesCoefficients;
using rnn_vad::ComputeOpusBandBoundaries;
using rnn_vad::kHpfConfig48kHz;
using rnn_vad::kFrameSize10ms24kHz;
using rnn_vad::kFrameSize20ms24kHz;
using rnn_vad::kNumOpusBands;
using rnn_vad::kPitchMaxPeriod24kHz;
using rnn_vad::kSampleRate24kHz;
using rnn_vad::RnnVadFft;
using rnn_vad::SequenceBuffer;
using rnn_vad::SpectralFeaturesExtractor;

namespace {

#if 0
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
#endif

}  // namespace

// Check that when using precomputed FFT coefficients for frames at 48 kHz, the
// output of ComputeBandEnergies() is within tolerance when compared to that of
// the reference code.
// TEST(RnnVadTest, ComputeBandEnergies48kHzWithinTolerance) {
//   constexpr size_t num_fft_points = kFrameSize20ms48kHz / 2 + 1;
//   // Init input data reader and buffers.
//   auto fft_coeffs_reader = CreateFftCoeffsReader();
//   const size_t num_frames = fft_coeffs_reader.second;
//   ASSERT_EQ(
//       num_fft_points,
//       rtc::CheckedDivExact(fft_coeffs_reader.first->data_length(),
//       num_frames) /
//           2);
//   std::array<float, num_fft_points> fft_coeffs_real;
//   std::array<float, num_fft_points> fft_coeffs_imag;
//   AlignedArray<std::complex<float>> fft_coeffs(
//       1, num_fft_points, RealFourier::kFftBufferAlignment);
//   // Init expected output reader and buffer.
//   auto band_energies_reader = CreateBandEnergyCoeffsReader();
//   ASSERT_EQ(num_frames, band_energies_reader.second);
//   std::array<float, kNumOpusBands> expected_band_energies;
//   // Init band energies coefficients computation.
//   const auto band_boundary_indexes =
//       ComputeOpusBandBoundaries(kSampleRate48kHz, kFrameSize20ms48kHz);
//   std::array<float, kNumOpusBands> computed_band_energies;

//   // The read FFT coefficients must be adapted to the expected FFT scaling
//   // scheme.
//   const float fft_coeffs_scaling = static_cast<float>(kFrameSize20ms48kHz);

//   // Check output for every frame.
//   {
//     FloatingPointExceptionObserver fpe_observer;
//     for (size_t i = 0; i < num_frames; ++i) {
//       SCOPED_TRACE(i);
//       // Read input.
//       fft_coeffs_reader.first->ReadChunk(
//           {fft_coeffs_real.data(), fft_coeffs_real.size()});
//       fft_coeffs_reader.first->ReadChunk(
//           {fft_coeffs_imag.data(), fft_coeffs_imag.size()});
//       for (size_t i = 0; i < num_fft_points; ++i) {
//         fft_coeffs.Row(0)[i] =
//             fft_coeffs_scaling *
//             std::complex<float>(fft_coeffs_real[i], fft_coeffs_imag[i]);
//       }
//       band_energies_reader.first->ReadChunk(
//           {expected_band_energies.data(), expected_band_energies.size()});
//       // Compute band energy coefficients and check output.
//       ComputeBandEnergies({fft_coeffs.Row(0), num_fft_points},
//                           {band_boundary_indexes}, {computed_band_energies});
//       ExpectNearRelative(expected_band_energies, computed_band_energies,
//       1e-6f);
//     }
//   }
// }

// This test performs the same verification of
// RnnVad.ComputeBandEnergies48kHzWithinTolerance() but, instead of using
// pre-computed FFT coefficients, it computes them.
// TEST(RnnVadTest, ComputeFftAndBandEnergies48kHzWithinTolerance) {
//   // PCM samples reader and buffers.
//   auto samples_reader = CreatePcmSamplesReader(kFrameSize10ms48kHz);
//   const size_t num_frames = samples_reader.second;
//   std::array<float, kFrameSize10ms48kHz> samples;
//   rtc::ArrayView<float, kFrameSize10ms48kHz> samples_view(
//       samples.data(), kFrameSize10ms48kHz);
//   // Read ground-truth.
//   auto band_energies_reader = CreateBandEnergyCoeffsReader();
//   ASSERT_EQ(num_frames, band_energies_reader.second);
//   std::array<float, kNumOpusBands> expected_band_energies;
//   // Init pipeline.
//   SequenceBuffer<float, kFrameSize20ms48kHz, kFrameSize10ms48kHz>
//   seq_buf(0.f); auto seq_buf_view = seq_buf.GetBufferView(); BiQuadFilter
//   hpf(kHpfConfig48kHz); RnnVadFft fft(kFrameSize20ms48kHz); auto
//   fft_output_buf_view = fft.GetFftOutputBufferView(); const auto
//   band_boundary_indexes =
//       ComputeOpusBandBoundaries(kSampleRate48kHz, kFftLenght20ms48kHz);
//   std::array<float, kNumOpusBands> computed_band_energies;

//   // Process frames.
//   {
//     FloatingPointExceptionObserver fpe_observer;
//     for (size_t i = 0; i < num_frames; ++i) {
//       SCOPED_TRACE(i);
//       // Read 10 ms audio frame and corresponding band energies.
//       samples_reader.first->ReadChunk(samples_view);
//       band_energies_reader.first->ReadChunk(
//           {expected_band_energies.data(), expected_band_energies.size()});
//       // Run pipeline and check output.
//       hpf.ProcessFrame(samples_view, samples_view);
//       seq_buf.Push(samples_view);
//       fft.ForwardFft(seq_buf_view);
//       ComputeBandEnergies(fft_output_buf_view, {band_boundary_indexes},
//                           {computed_band_energies});
//       ExpectNearRelative(expected_band_energies, computed_band_energies,
//       1e-5f);
//       // CheckBandEnergies({expected_band_energies},
//       {computed_band_energies},
//       //                   0.1f, 1.5f);
//     }
//   }
// }

TEST(RnnVadTest, ComputeLogBandEnergiesCoefficientsBitExactness) {
  constexpr std::array<float, kNumOpusBands> input = {
      86.060539245605, 275.668334960938, 43.406528472900, 6.541896820068,
      17.964015960693, 8.090919494629,   1.261920094490,  1.212702631950,
      1.619154453278,  0.508935272694,   0.346316039562,  0.237035423517,
      0.172424271703,  0.271657168865,   0.126088857651,  0.139967113733,
      0.207200810313,  0.155893072486,   0.091090843081,  0.033391401172,
      0.013879744336,  0.011973354965};
  constexpr std::array<float, kNumOpusBands> expected_output = {
      1.934854507446,  2.440402746201,  1.637655138969,  0.816367030144,
      1.254645109177,  0.908534288406,  0.104459829628,  0.087320849299,
      0.211962252855,  -0.284886807203, -0.448164641857, -0.607240796089,
      -0.738917350769, -0.550279200077, -0.866177439690, -0.824003994465,
      -0.663138568401, -0.780171751976, -0.995288193226, -1.362596273422,
      -1.621970295906, -1.658103585243};
  std::array<float, kNumOpusBands> computed_output;
  ComputeLogBandEnergiesCoefficients({input}, {computed_output});
  ExpectNearAbsolute({expected_output}, {computed_output}, 1e-5f);
}

TEST(RnnVadTest, ComputeDctBitExactness) {
  constexpr std::array<float, kNumOpusBands> input = {
      0.232155621052,  0.678957760334, 0.220818966627,  -0.077363930643,
      -0.559227049351, 0.432545185089, 0.353900641203,  0.398993015289,
      0.409774333239,  0.454977899790, 0.300520688295,  -0.010286616161,
      0.272525429726,  0.098067551851, 0.083649002016,  0.046226885170,
      -0.033228103071, 0.144773483276, -0.117661058903, -0.005628800020,
      -0.009547689930, -0.045382082462};
  constexpr std::array<float, kNumOpusBands> expected_output = {
      0.697072803974,  0.442710995674,  -0.293156713247, -0.060711503029,
      0.292050391436,  0.489301353693,  0.402255415916,  0.134404733777,
      -0.086305990815, -0.199605688453, -0.234511867166, -0.413774639368,
      -0.388507157564, -0.032798115164, 0.044605545700,  0.112466648221,
      -0.050096966326, 0.045971218497,  -0.029815061018, -0.410366982222,
      -0.209233760834, -0.128037497401};
  const auto dct_table = ComputeDctTable();
  std::array<float, kNumOpusBands> computed_output;
  ComputeDct({input.data(), input.size()}, {dct_table.data(), dct_table.size()},
             ComputeDctScalingFactor(kNumOpusBands),
             {computed_output.data(), computed_output.size()});
  ExpectNearAbsolute({expected_output}, {computed_output}, 1e-5f);
}

// Check that the spectral coefficients difference between (i) those computed
// using the porting and (ii) those computed using the reference code is within
// a tolerance. While using the porting, a lower sample rate is used.
// TEST(RnnVadTest, CheckSpectralCoefficients) {
//   // PCM samples reader and buffers.
//   auto samples_reader = CreatePcmSamplesReader(kFrameSize10ms48kHz);
//   const size_t num_frames = samples_reader.second;
//   std::array<float, kFrameSize10ms48kHz> samples_48k_10ms;
//   // Read ground-truth.
//   auto spectral_coeffs_reader = CreateSpectralCoeffsReader();
//   const size_t expected_num_frames_without_silence =
//       spectral_coeffs_reader.second;
//   ASSERT_LE(expected_num_frames_without_silence, num_frames);
//   std::array<float, kNumOpusBands> expected_spectral_coeffs;
//   // Init pipeline.
//   ASSERT_EQ(kFrameSize10ms48kHz / 2, kFrameSize10ms24kHz);
//   ASSERT_EQ(kFrameSize20ms48kHz / 2, kFrameSize20ms24kHz);
//   std::array<float, kFrameSize10ms24kHz> samples_10ms_24kHz;
//   rtc::ArrayView<float, kFrameSize10ms24kHz> samples_10ms_24kHz_view(
//       samples_10ms_24kHz.data(), samples_10ms_24kHz.size());
//   PushSincResampler decimator(kFrameSize10ms48kHz, kFrameSize10ms24kHz);
//   SequenceBuffer<float, kFrameSize20ms24kHz, kFrameSize10ms24kHz>
//   seq_buf(0.f);
//   // BiQuadFilter hpf(kHpfConfig48kHz);
//   // BiQuadFilter hpf(
//   //     {-1.98889291, 0.98895425, 0.99446179, -1.98892358, 0.99446179});
//   SpectralFeaturesExtractor spectral_features_extractor;
//   std::array<float, kNumOpusBands> computed_spectral_coeffs;

//   FILE* fp_tmp =
//       fopen("/usr/local/google/home/alessiob/Desktop/ceps_computed.dat",
//       "wb");

//   // Process frames.
//   size_t num_frames_without_silence = 0;
//   {
//     FloatingPointExceptionObserver fpe_observer;

//     for (size_t i = 0; i < num_frames; ++i) {
//       SCOPED_TRACE(i);
//       // Read 10 ms audio frame.
//       samples_reader.first->ReadChunk({samples_48k_10ms});
//       // Run pipeline.
//       // hpf.ProcessFrame({samples_48k_10ms}, {samples_48k_10ms});
//       decimator.Resample(samples_48k_10ms.data(), samples_48k_10ms.size(),
//                          samples_10ms_24kHz.data(),
//                          samples_10ms_24kHz.size());
//       seq_buf.Push(samples_10ms_24kHz_view);
//       // In this test, the feature extractor is only used to compute spectral
//       // coefficients; therefore, it is ok to pass the same frame for both
//       // reference and lagged frames below.
//       auto reference_frame_view =
//           seq_buf.GetBufferView(kPitchMaxPeriod24kHz, kFrameSize20ms24kHz);
//       const bool is_silence =
//       spectral_features_extractor.AnalyzeCheckSilence(
//           {reference_frame_view.data(), kFrameSize20ms24kHz},
//           {reference_frame_view.data(), kFrameSize20ms24kHz});
//       if (is_silence)
//         continue;
//       num_frames_without_silence++;
//       // Read ground truth if there is no silence. Reading here is used to
//       // implicitly check that silence is detected correctly.
//       spectral_coeffs_reader.first->ReadChunk(
//           {expected_spectral_coeffs.data(),
//           expected_spectral_coeffs.size()});
//       // Get spectral coefficients and check output.
//       spectral_features_extractor.CopySpectralCoefficients(
//           {computed_spectral_coeffs});
//       ExpectNearAbsolute({expected_spectral_coeffs},
//       {computed_spectral_coeffs},
//                          5.f);
//       // TODO(alessiob): Visually inspect, fix and reduce tolerance.
//       fwrite(computed_spectral_coeffs.data(), sizeof(float),
//              computed_spectral_coeffs.size(), fp_tmp);
//     }
//   }
//   EXPECT_EQ(expected_num_frames_without_silence, num_frames_without_silence);

//   fclose(fp_tmp);
// }

}  // namespace test
}  // namespace webrtc
