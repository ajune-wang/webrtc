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

#include <cmath>

#include "rtc_base/checks.h"
#include "rtc_base/ptr_util.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

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
  ASSERT_EQ(expected.size(), computed.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    if (expected[i] == 0.f && computed[i] == 0.f)
      continue;
    SCOPED_TRACE(i);
    const float relative_err =
        std::fabs(expected[i] - computed[i]) /
        std::max(std::fabs(expected[i]), std::fabs(computed[i]));
    EXPECT_LE(relative_err, tolerance);
  }
}

std::pair<std::unique_ptr<BinaryFileReader<int16_t, float>>, const size_t>
CreatePcmSamplesReader(const size_t frame_length) {
  auto ptr = rtc::MakeUnique<BinaryFileReader<int16_t, float>>(
      test::ResourcePath("common_audio/rnn_vad/samples", "pcm"), frame_length);
  // The last incomplete frame is ignored.
  return {std::move(ptr), ptr->data_length() / frame_length};
}

std::pair<std::unique_ptr<BinaryFileReader<float>>, const size_t>
CreateLpResidualReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/lp_res", "dat"), 864);
  // LP residual vectors (864), pitch period and pitch gain.
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(866))};
}

std::pair<std::unique_ptr<BinaryFileReader<float>>, const size_t>
CreateFftCoeffsReader() {
  constexpr size_t num_fft_points = 481;
  constexpr size_t row_size = 2 * num_fft_points;  // Real and imaginary values.
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/fft", "dat"), num_fft_points);
  return {std::move(ptr), rtc::CheckedDivExact(ptr->data_length(), row_size)};
}

std::pair<std::unique_ptr<BinaryFileReader<float>>, const size_t>
CreateBandEnergyCoeffsReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/band_energies", "dat"), 22);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(22))};
}

std::pair<std::unique_ptr<BinaryFileReader<float>>, const size_t>
CreateFeatureMatrixReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/features", "out"), 42);
  // Features (42) and silence flag.
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(43))};
}

std::pair<std::unique_ptr<BinaryFileReader<float>>, const size_t>
CreateVadProbsReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      test::ResourcePath("common_audio/rnn_vad/vad_prob", "out"));
  return {std::move(ptr), ptr->data_length()};
}

}  // namespace test
}  // namespace webrtc
