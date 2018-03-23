/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/lp_residual.h"

#include <array>
#include <cstring>
#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// Compute cross-correlation coefficients between |x| and |y| and writes them in
// |x_corr|. The lag values are in {0, ..., max_lag - 1}, where max_lag equals
// the size of |x_corr|.
// The |x| and |y| sub-arrays used to compute a cross-correlation coefficients
// for a lag l have both size "size of |x| - l" - i.e., the longest sub-array is
// used. |x| and |y| must have the same size.
void ComputeCrossCorrelation(rtc::ArrayView<const float> x,
                             rtc::ArrayView<const float> y,
                             rtc::ArrayView<float> x_corr) {
  const size_t max_lag = x_corr.size();
  RTC_DCHECK_EQ(x.size(), y.size());
  RTC_DCHECK_LT(max_lag, x.size());
  // TODO(alessiob): Maybe optimize using vectorization.
  for (size_t lag = 0; lag < max_lag; ++lag)
    x_corr[lag] =
        std::inner_product(x.begin(), x.end() - lag, y.begin() + lag, 0.f);
}

}  // namespace

void ComputeInverseFilterCoefficients(
    rtc::ArrayView<const float> x,
    rtc::ArrayView<float, kNumLpcCoefficients> lpc_coeffs) {
  // Init.
  std::memset(lpc_coeffs.data(), 0.f, lpc_coeffs.size());
  std::array<float, kNumLpcCoefficients> auto_corr;
  // Compute auto-correlation terms and denoise them assuming -40 dB white
  // noise floor.
  ComputeCrossCorrelation(x, x, {auto_corr});
  auto_corr[0] *= 1.0001f;
  for (size_t i = 0; i < kNumLpcCoefficients; ++i)
    auto_corr[i] -= auto_corr[i] * (0.008f * i) * (0.008f * i);
  if (auto_corr[0] == 0.f)  // Empty frame case.
    return;
  // Compute inverse filter coefficients.
  // One extra coefficient will be added later (see below).
  float lpc_coeffs_pre[kNumLpcCoefficients - 1];
  float error = auto_corr[0];
  for (size_t i = 0; i < kNumLpcCoefficients - 1; ++i) {
    float reflection_coeff = 0.f;
    for (size_t j = 0; j < i; ++j)
      reflection_coeff += lpc_coeffs_pre[j] * auto_corr[i - j];
    reflection_coeff += auto_corr[i + 1];
    reflection_coeff /= -error;
    // Update LPC coefficients and total error.
    lpc_coeffs_pre[i] = reflection_coeff;
    for (size_t j = 0; j<(i + 1)>> 1; ++j) {
      const float tmp1 = lpc_coeffs_pre[j];
      const float tmp2 = lpc_coeffs_pre[i - 1 - j];
      lpc_coeffs_pre[j] = tmp1 + reflection_coeff * tmp2;
      lpc_coeffs_pre[i - 1 - j] = tmp2 + reflection_coeff * tmp1;
    }
    error -= reflection_coeff * reflection_coeff * error;
    if (error < 0.001f * auto_corr[0])
      break;
  }
  // LPC coefficients smoothing (low-pass filter).
  {
    float c = 1.f;
    for (size_t i = 0; i < kNumLpcCoefficients - 1; ++i) {
      c *= 0.9f;
      lpc_coeffs_pre[i] *= c;
    }
  }
  // Add a zero to account for lip radiation while applying another smoothing
  // step.
  {
    const float c = 0.8f;
    lpc_coeffs[0] = lpc_coeffs_pre[0] + c;
    lpc_coeffs[1] = lpc_coeffs_pre[1] + c * lpc_coeffs_pre[0];
    lpc_coeffs[2] = lpc_coeffs_pre[2] + c * lpc_coeffs_pre[1];
    lpc_coeffs[3] = lpc_coeffs_pre[3] + c * lpc_coeffs_pre[2];
    lpc_coeffs[4] = c * lpc_coeffs_pre[3];
  }
}

void ComputeLpResidual(
    rtc::ArrayView<const float, kNumLpcCoefficients> lpc_coeffs,
    rtc::ArrayView<const float> x,
    rtc::ArrayView<float> y) {
  RTC_DCHECK_LT(kNumLpcCoefficients, x.size());
  RTC_DCHECK_EQ(x.size(), y.size());
  std::array<float, kNumLpcCoefficients> input_chunk;
  input_chunk.fill(0.f);
  for (size_t i = 0; i < y.size(); ++i) {
    float sum = std::inner_product(input_chunk.begin(), input_chunk.end(),
                                   lpc_coeffs.begin(), x[i]);
    // Circular shift and add a new sample.
    for (size_t j = kNumLpcCoefficients - 1; j > 0; --j)
      input_chunk[j] = input_chunk[j - 1];
    input_chunk[0] = x[i];
    // Copy result.
    y[i] = sum;
  }
}

}  // namespace rnn_vad
}  // namespace webrtc
