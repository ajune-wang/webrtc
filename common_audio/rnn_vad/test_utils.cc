/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/test_utils.h"

#include "rtc_base/checks.h"
#include "rtc_base/ptr_util.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {
namespace {

using ReaderPairType =
    std::pair<std::unique_ptr<BinaryFileReader<float>>, const size_t>;

}  // namespace

void ExpectEqualFloatArray(rtc::ArrayView<const float> expected,
                           rtc::ArrayView<const float> computed) {
  ASSERT_EQ(expected.size(), computed.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_FLOAT_EQ(expected[i], computed[i]);
  }
}

void ExpectNearAbsolute(rtc::ArrayView<const float> expected,
                        rtc::ArrayView<const float> computed,
                        const float tolerance) {
  ASSERT_EQ(expected.size(), computed.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_NEAR(expected[i], computed[i], tolerance);
  }
}

void ExpectNearRelative(rtc::ArrayView<const float> expected,
                        rtc::ArrayView<const float> computed,
                        const float tolerance) {
  // The relative error is undefined when the expected value is 0.
  // When that happens, check the absolute error instead. |safe_den| is used
  // below to implement such logic.
  auto safe_den = [](float x) { return (x == 0.f) ? 1.f : std::fabs(x); };
  ASSERT_EQ(expected.size(), computed.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    const float abs_diff = std::fabs(expected[i] - computed[i]);
    // No failure when the values are equal.
    if (abs_diff == 0.f)
      continue;
    SCOPED_TRACE(i);
    SCOPED_TRACE(expected[i]);
    SCOPED_TRACE(computed[i]);
    EXPECT_LE(abs_diff / safe_den(expected[i]), tolerance);
  }
}

std::pair<std::unique_ptr<BinaryFileReader<int16_t, float>>, const size_t>
CreatePcmSamplesReader(const size_t frame_length) {
  auto ptr = rtc::MakeUnique<BinaryFileReader<int16_t, float>>(
      test::ResourcePath("common_audio/rnn_vad/samples", "pcm"), frame_length);
  // The last incomplete frame is ignored.
  return {std::move(ptr), ptr->data_length() / frame_length};
}

ReaderPairType CreatePreprocessedSamplesReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/pcm_hpf", "dat"), 480);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(480))};
}

ReaderPairType CreateAnalysisBufFirstLastReader() {
  // TODO(alessiob): Remove file in resources when this function is deleted.
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/analysis_buf_20ms_first_last",
                         "dat"),
      2);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(2))};
}

ReaderPairType CreatePitchBuffer24kHzReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/pitch_buf_24k", "dat"), 864);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(864))};
}

ReaderPairType CreateLpResidualReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/lp_res", "dat"), 864);
  // LP residual vectors (864), pitch period and pitch gain.
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(866))};
}

ReaderPairType CreateFftCoeffsReader(bool after_hpf) {
  constexpr size_t num_fft_points = 481;
  constexpr size_t row_size = 2 * num_fft_points;  // Real and imaginary values.
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath(after_hpf ? "common_audio/rnn_vad/fft"
                                   : "common_audio/rnn_vad/fft_no_hpf",
                         "dat"),
      num_fft_points);
  return {std::move(ptr), rtc::CheckedDivExact(ptr->data_length(), row_size)};
}

ReaderPairType CreateBandEnergyCoeffsReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/band_energies", "dat"), 22);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(22))};
}

ReaderPairType CreateFeatureMatrixReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/features", "out"), 42);
  // Features (42) and silence flag.
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(43))};
}

ReaderPairType CreateVadProbsReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/vad_prob", "out"));
  return {std::move(ptr), ptr->data_length()};
}

ReaderPairType CreateSpectralCoeffsReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/ceps", "dat"), 22);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(22))};
}

ReaderPairType CreateSpectralCoeffsAvgReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/ceps_avg", "dat"), 6);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(6))};
}

ReaderPairType CreateSpectralCoeffsDelta1Reader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/ceps_d1", "dat"), 6);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(6))};
}

ReaderPairType CreateSpectralCoeffsDelta2Reader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/ceps_d2", "dat"), 6);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(6))};
}

ReaderPairType CreateSpectralVariabilityReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/spec_variability", "out"));
  return {std::move(ptr), ptr->data_length()};
}

}  // namespace test
}  // namespace webrtc
