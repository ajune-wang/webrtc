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
using rnn_vad::ComputeOpusBandsIndexes;
using rnn_vad::Decimate48k24k;
using rnn_vad::kInputFrameSize;
using rnn_vad::kFeatureVectorSize;
using rnn_vad::kNumOpusBands;
using rnn_vad::RnnVadFeaturesExtractor;
using rnn_vad::RnnVadFft;
using rnn_vad::SequenceBuffer;

namespace {

// TODO(alessiob): Remove if unused.

}  // namespace

// Compare ComputeBandEnergies against the reference code.
TEST(RnnVad, ComputeBandEnergiesBitExactness) {
  constexpr size_t num_fft_points = k48k20msFrameSize / 2 + 1;
  // Init input data reader and buffers.
  BinaryFileReader<float> fft_coeffs_reader(
      test::ResourcePath("common_audio/rnn_vad/fft", "dat"), num_fft_points);
  const size_t num_frames =
      rtc::CheckedDivExact(fft_coeffs_reader.data_length(), 2 * num_fft_points);
  std::array<float, num_fft_points> fft_coeffs_real;
  std::array<float, num_fft_points> fft_coeffs_imag;
  AlignedArray<std::complex<float>> fft_coeffs(
      1, num_fft_points, RealFourier::kFftBufferAlignment);
  // Init expected output reader and buffer.
  BinaryFileReader<float> band_energies_reader(
      test::ResourcePath("common_audio/rnn_vad/band_energies", "dat"),
      kNumOpusBands);
  ASSERT_EQ(num_frames, rtc::CheckedDivExact(band_energies_reader.data_length(),
                                             kNumOpusBands));
  std::array<float, kNumOpusBands> expected_band_energies;
  // Init band energies coefficients computation.
  const auto band_boundary_indexes =
      ComputeOpusBandsIndexes(kSampleRate48k, k48k20msFrameSize);
  std::array<float, kNumOpusBands> computed_band_energies;

  // Check output for every frame.
  for (size_t i = 0; i < num_frames; ++i) {
    SCOPED_TRACE(i);
    // Read input.
    fft_coeffs_reader.ReadChunk(
        {fft_coeffs_real.data(), fft_coeffs_real.size()});
    fft_coeffs_reader.ReadChunk(
        {fft_coeffs_imag.data(), fft_coeffs_imag.size()});
    for (size_t i = 0; i < num_fft_points; ++i) {
      fft_coeffs.Row(0)[i] =
          std::complex<float>(fft_coeffs_real[i], fft_coeffs_imag[i]);
    }
    band_energies_reader.ReadChunk(
        {expected_band_energies.data(), expected_band_energies.size()});
    // Compute band energy coefficients and check output.
    ComputeBandEnergies({fft_coeffs.Row(0), num_fft_points}, 1.f,
                        {band_boundary_indexes}, {computed_band_energies});
    EXPECT_EQ(expected_band_energies, computed_band_energies);
  }
}

// Check that the band energy coefficients difference between (i) those computed
// using the porting and (ii) those computed using the reference code is within
// a tolerance.

// TEST(RnnVad, CheckComputeBandEnergiesPipeline) {
//   // Read ground-truth.
//   BinaryFileReader<float> band_energies_reader(
//       test::ResourcePath("common_audio/rnn_vad/band_energies", "dat"),
//       kNumOpusBands);
//   std::array<float, kNumOpusBands> expected_band_energies;
//   // PCM samples reader and buffers.
//   BinaryFileReader<int16_t, float> samples_reader(
//       test::ResourcePath("common_audio/rnn_vad/samples", "pcm"),
//       k48k10msFrameSize);
//   std::array<float, k48k10msFrameSize> samples;
//   rtc::ArrayView<float, k48k10msFrameSize> samples_view(samples.data(),
//                                                         k48k10msFrameSize);
//   SequenceBuffer<float, k48k20msFrameSize, k48k10msFrameSize> seq_buf(0.f);
//   auto seq_buf_view = seq_buf.GetBufferView();

//   // Init HP filter.
//   HighPassFilter hpf(k48kHpfConfig);
//   hpf.SetState(k48kHpfInitialState);
//   // Init FFT.
//   RnnVadFft fft(k48k20msFrameSize);
//   const auto fft_output_buf_view = fft.GetFftOutputBufferView();
//   // Init band energies computation.
//   const auto band_boundary_indexes =
//       ComputeOpusBandsIndexes(kSampleRate48k, k48k20msFftSize);
//   std::array<float, kNumOpusBands> computed_band_energies;

//   // Process frames. The last one is discarded if incomplete.
//   auto relative_err = [](float a, float b) { return std::fabs(a - b) / a; };
//   const size_t num_frames = samples_reader.data_length() / k48k10msFrameSize;
//   ASSERT_EQ(num_frames * kNumOpusBands, band_energies_reader.data_length());
//   ASSERT_EQ(num_frames, kExpectedBandEnergySums.size());
//   for (size_t i = 0; i < num_frames; ++i) {
//     std::ostringstream ss;
//     ss << "frame " << i;
//     SCOPED_TRACE(ss.str());
//     // Read 10 ms audio frame and corresponding band energies.
//     samples_reader.ReadChunk(samples_view);
//     band_energies_reader.ReadChunk(
//         {expected_band_energies.data(), expected_band_energies.size()});
//     // Apply high-pass filter to the two 10 ms frame.
//     hpf.ProcessFrame(samples_view, samples_view);
//     // Push the filtered 10 ms frame into the sequence buffer.
//     seq_buf.Push({samples.data(), samples.size()});
//     // Compute FFT.
//     fft.ForwardFft(seq_buf_view);
//     // Compute band energies.
//     ComputeBandEnergies(fft_output_buf_view, 1.f / k48k20msFftSize,
//                         {band_boundary_indexes}, {computed_band_energies});
//     // Check sum of band energies.
//     const float expected_band_energies_sum = std::accumulate(
//         expected_band_energies.begin(), expected_band_energies.end(), 0.f);
//     if (expected_band_energies_sum == 0.f) {
//       ASSERT_EQ(0.f, kExpectedBandEnergySums[i]);
//       continue;
//     }
//     ASSERT_LT(
//         relative_err(kExpectedBandEnergySums[i], expected_band_energies_sum),
//         0.095f)
//         << "The relative error caused by FFT zero-padding is too high.";
//     const float computed_band_energies_sum = std::accumulate(
//         computed_band_energies.begin(), computed_band_energies.end(), 0.f);
//     // Compare to both zero-padded and non-padded versions. In the first
//     case,
//     // a lower threshold is used since both values correspond to the
//     // zero-padding case.
//     EXPECT_NEAR(kExpectedBandEnergySums[i], computed_band_energies_sum,
//     1e-3f);
//     // EXPECT_NEAR(expected_band_energies_sum, computed_band_energies_sum,
//     //             1e-1f);
//     // Check band energies.
//     // ExpectNear({expected_band_energies}, {computed_band_energies}, 1e-1f);
//     // TODO(alessiob): Remove once debugged.
//     if (i == 23)
//       break;
//   }
// }

// Check that the RNN VAD features difference between (i) those computed
// using the porting and (ii) those computed using the reference code is within
// a tolerance.

// TODO(alessiob): Enable once feature extraction is fully implemented.
// TEST(RnnVad, DISABLED_FeaturesExtractorBitExactness) {
// // PCM samples reader and buffers.
// BinaryFileReader<int16_t, float> samples_reader(
//     test::ResourcePath("common_audio/rnn_vad/samples", "pcm"),
// kInputAudioFrameSize);
// std::array<float, kInputAudioFrameSize> samples;
// std::array<float, kInputFrameSize> samples_decimated;
// // Features reader and buffers.
// BinaryFileReader<float> features_reader(
//     test::ResourcePath("common_audio/rnn_vad/features", "out"),
//  kFeatureVectorSize);
// float is_silence;
// std::array<float, kFeatureVectorSize> features;
// rtc::ArrayView<const float, kFeatureVectorSize> expected_features_view(
//     features.data(), features.size());
// // Feature extractor.
// RnnVadFeaturesExtractor features_extractor;
// auto extracted_features_view = features_extractor.GetOutput();
// // Process frames. The last one is discarded if incomplete.
// const size_t num_frames = samples_reader.data_length() / kInputFrameSize;
// for (size_t i = 0; i < num_frames; ++i) {
//   std::ostringstream ss;
//   ss << "frame " << i;
//   SCOPED_TRACE(ss.str());
//   // Read and downsample audio frame.
//   samples_reader.ReadChunk({samples.data(), samples.size()});
//   Decimate48k24k({samples_decimated.data(), samples_decimated.size()},
//                  {samples.data(), samples.size()});
//   // Compute feature vector.
//   features_extractor.ComputeFeatures(
//       {samples_decimated.data(), samples_decimated.size()});
//   // Read expected feature vector.
//   RTC_CHECK(features_reader.ReadValue(&is_silence));
//   RTC_CHECK(features_reader.ReadChunk({features.data(), features.size()}));
//   // Check silence flag and feature vector.
//   EXPECT_EQ(is_silence, features_extractor.is_silence());
//   ExpectNear(expected_features_view, extracted_features_view);
// }
// }

}  // namespace test
}  // namespace webrtc
