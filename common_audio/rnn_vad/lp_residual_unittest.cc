/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>

#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/lp_residual.h"
#include "common_audio/rnn_vad/test_utils.h"
#include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

using rnn_vad::MaybeComputeInverseFilterCoefficients;
using rnn_vad::ComputeLpResidual;
using rnn_vad::kBufSize24kHz;
using rnn_vad::kBufSize48kHz;
using rnn_vad::kFrameSize10ms24kHz;
using rnn_vad::kFrameSize10ms48kHz;
using rnn_vad::kNumLpcCoefficients;
using rnn_vad::kPitchMaxPeriod48kHz;

TEST(RnnVadTest, CheckEmptyFrame) {
  {
    FloatingPointExceptionObserver fpe_observer;

    // Input frame (empty, i.e., all samples set to 0).
    std::array<float, kFrameSize10ms24kHz> empty_frame;
    empty_frame.fill(0.f);
    // Compute inverse filter coefficients.
    std::array<float, kNumLpcCoefficients> lpc_coeffs;
    MaybeComputeInverseFilterCoefficients(
        {empty_frame}, {lpc_coeffs.data(), lpc_coeffs.size()});
    // Compute LP residual.
    std::array<float, kFrameSize10ms24kHz> lp_residual;
    ComputeLpResidual({lpc_coeffs.data(), lpc_coeffs.size()}, {empty_frame},
                      {lp_residual});
  }
}

TEST(RnnVadTest, LpResidualPipelineBitExactness) {
  // Pitch buffer 24 kHz data reader.
  auto pitch_buf_24kHz_reader = CreatePitchBuffer24kHzReader();
  const size_t num_frames = pitch_buf_24kHz_reader.second;
  std::array<float, kBufSize24kHz> pitch_buf_data;
  rtc::ArrayView<float, kBufSize24kHz> pitch_buf_data_view(
      pitch_buf_data.data(), pitch_buf_data.size());
  // Read ground-truth.
  auto lp_residual_reader = CreateLpResidualReader();
  ASSERT_EQ(num_frames, lp_residual_reader.second);
  std::array<float, kBufSize24kHz> expected_lp_residual;
  rtc::ArrayView<float, kBufSize24kHz> expected_lp_residual_view(
      expected_lp_residual.data(), expected_lp_residual.size());
  // Init pipeline.
  std::array<float, kNumLpcCoefficients> lpc_coeffs;
  rtc::ArrayView<float, kNumLpcCoefficients> lpc_coeffs_view(
      lpc_coeffs.data(), kNumLpcCoefficients);
  std::array<float, kBufSize24kHz> computed_lp_residual;
  rtc::ArrayView<float, kBufSize24kHz> computed_lp_residual_view(
      computed_lp_residual.data(), computed_lp_residual.size());
  {
    FloatingPointExceptionObserver fpe_observer;

    for (size_t i = 0; i < num_frames; ++i) {
      SCOPED_TRACE(i);
      // Read input and expected output.
      pitch_buf_24kHz_reader.first->ReadChunk(pitch_buf_data_view);
      lp_residual_reader.first->ReadChunk(expected_lp_residual_view);
      // Skip pitch gain and period.
      // TODO(alessiob): This is error prone. Rename reader.
      float unused;
      lp_residual_reader.first->ReadValue(&unused);
      lp_residual_reader.first->ReadValue(&unused);
      // Run pipeline.
      MaybeComputeInverseFilterCoefficients(pitch_buf_data_view,
                                            lpc_coeffs_view);
      ComputeLpResidual(lpc_coeffs_view, pitch_buf_data_view,
                        computed_lp_residual_view);
      // Compare.
      ExpectNearAbsolute(expected_lp_residual_view, computed_lp_residual_view,
                         kFloatMin);
    }
  }
}

}  // namespace test
}  // namespace webrtc
