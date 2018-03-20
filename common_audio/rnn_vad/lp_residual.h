/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_LP_RESIDUAL_H_
#define COMMON_AUDIO_RNN_VAD_LP_RESIDUAL_H_

#include "api/array_view.h"

namespace webrtc {
namespace rnn_vad {

// LPC inverse filter length.
constexpr size_t kNumLpcCoefficients = 5;

// Compute cross-correlation coefficients between |x| and |y| for lags in
// {0, ..., |x_corr| - 1}.
void ComputeInverseFilterCoefficients(
    rtc::ArrayView<const float> x,
    rtc::ArrayView<float, kNumLpcCoefficients> lpc_coeffs);

// Compute the LP residual for the input frame |x| and the LPC coefficients
// |lpc_coeffs|. |y| and |x| can point to the same array for in-place
// computation.
void ComputeLpResidual(
    rtc::ArrayView<const float, kNumLpcCoefficients> lpc_coeffs,
    rtc::ArrayView<const float> x,
    rtc::ArrayView<float> y);

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_LP_RESIDUAL_H_
