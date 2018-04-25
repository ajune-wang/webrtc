/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"

#include <cmath>

#include "rtc_base/checks.h"
#include "rtc_base/ptr_util.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace rnn_vad {
namespace test {
namespace {

using ReaderPairType =
    std::pair<std::unique_ptr<BinaryFileReader<float>>, const size_t>;

}  // namespace

using webrtc::test::ResourcePath;

void ExpectNearAbsolute(rtc::ArrayView<const float> expected,
                        rtc::ArrayView<const float> computed,
                        float tolerance) {
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
      ResourcePath("audio_processing/agc2/rnn_vad/samples", "pcm"),
      frame_length);
  // The last incomplete frame is ignored.
  return {std::move(ptr), ptr->data_length() / frame_length};
}

ReaderPairType CreatePitchBuffer24kHzReader() {
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      ResourcePath("audio_processing/agc2/rnn_vad/pitch_buf_24k", "dat"), 864);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), static_cast<size_t>(864))};
}

ReaderPairType CreateLpResidualAndPitchPeriodGainReader() {
  constexpr size_t num_lp_residual_coeffs = 864;
  auto ptr = rtc::MakeUnique<BinaryFileReader<float>>(
      ResourcePath("audio_processing/agc2/rnn_vad/pitch_lp_res", "dat"),
      num_lp_residual_coeffs);
  return {std::move(ptr),
          rtc::CheckedDivExact(ptr->data_length(), 2 + num_lp_residual_coeffs)};
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
