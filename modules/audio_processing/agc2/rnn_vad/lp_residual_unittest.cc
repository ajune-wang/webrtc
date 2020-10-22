/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/lp_residual.h"

#include <algorithm>
#include <array>
#include <vector>

#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "modules/audio_processing/test/performance_timer.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace test {

// Checks that the LP residual can be computed on an empty frame.
TEST(RnnVadTest, LpResidualOfEmptyFrame) {
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;

  // Input frame (empty, i.e., all samples set to 0).
  std::array<float, kFrameSize10ms24kHz> empty_frame;
  empty_frame.fill(0.f);
  // Compute inverse filter coefficients.
  std::array<float, kNumLpcCoefficients> lpc_coeffs;
  ComputeAndPostProcessLpcCoefficients(empty_frame, lpc_coeffs);
  // Compute LP residual.
  std::array<float, kFrameSize10ms24kHz> lp_residual;
  ComputeLpResidual(lpc_coeffs, empty_frame, lp_residual);
}

// Checks that the computed LP residual is bit-exact given test input data.
TEST(RnnVadTest, LpResidualPipelineBitExactness) {
  // Input and expected output readers.
  auto pitch_buf_24kHz_reader = CreatePitchBuffer24kHzReader();
  auto lp_residual_reader = CreateLpResidualAndPitchPeriodGainReader();

  // Buffers.
  std::vector<float> pitch_buf_data(kBufSize24kHz);
  std::array<float, kNumLpcCoefficients> lpc_coeffs;
  std::vector<float> computed_lp_residual(kBufSize24kHz);
  std::vector<float> expected_lp_residual(kBufSize24kHz);

  // Test length.
  const size_t num_frames = std::min(pitch_buf_24kHz_reader.second,
                                     static_cast<size_t>(300));  // Max 3 s.
  ASSERT_GE(lp_residual_reader.second, num_frames);

  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    for (size_t i = 0; i < num_frames; ++i) {
      // Read input.
      ASSERT_TRUE(pitch_buf_24kHz_reader.first->ReadChunk(pitch_buf_data));
      // Read expected output (ignore pitch gain and period).
      ASSERT_TRUE(lp_residual_reader.first->ReadChunk(expected_lp_residual));
      float unused;
      ASSERT_TRUE(lp_residual_reader.first->ReadValue(&unused));
      ASSERT_TRUE(lp_residual_reader.first->ReadValue(&unused));

      // Check every 200 ms.
      if (i % 20 != 0) {
        continue;
      }

      SCOPED_TRACE(i);
      ComputeAndPostProcessLpcCoefficients(pitch_buf_data, lpc_coeffs);
      ComputeLpResidual(lpc_coeffs, pitch_buf_data, computed_lp_residual);
      ExpectNearAbsolute(expected_lp_residual, computed_lp_residual, kFloatMin);
    }
  }
}

TEST(RnnVadTest, DISABLED_ComputeLpResidualBenchmark) {
  // Prefetch test data.
  auto pitch_buf_24kHz_reader = CreatePitchBuffer24kHzReader();
  std::vector<std::array<float, kBufSize24kHz>> pitch_buffers(
      pitch_buf_24kHz_reader.second);
  for (auto& v : pitch_buffers) {
    ASSERT_TRUE(pitch_buf_24kHz_reader.first->ReadChunk(v));
  }

  // Pre-compute LPC coefficients.
  std::vector<std::array<float, kNumLpcCoefficients>> lpc_coeffs(
      pitch_buffers.size());
  for (int i = 0; rtc::SafeLt(i, lpc_coeffs.size()); ++i) {
    ComputeAndPostProcessLpcCoefficients(pitch_buffers[i], lpc_coeffs[i]);
  }

  constexpr int kNumberOfTests = 1000;
  constexpr int kInnerLoopLength = 50;
  RTC_LOG(LS_INFO) << kNumberOfTests << " x " << kInnerLoopLength << " x "
                   << pitch_buffers.size() << " tests";

  // Output.
  std::vector<float> lp_residual(kBufSize24kHz);

  ::webrtc::test::PerformanceTimer perf_timer(kNumberOfTests);
  for (int i = 0; i < kNumberOfTests; ++i) {
    perf_timer.StartTimer();
    for (int k = 0; k < kInnerLoopLength; ++k) {
      for (int j = 0; rtc::SafeLt(j, pitch_buffers.size()); ++j) {
        ComputeLpResidual(lpc_coeffs[i], pitch_buffers[i], lp_residual);
      }
    }
    perf_timer.StopTimer();
  }

  RTC_LOG(LS_INFO) << "ComputeLpResidual " << perf_timer.GetDurationAverage()
                   << " us +/-" << perf_timer.GetDurationStandardDeviation();
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
