/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_VECTOR_MATH_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_VECTOR_MATH_H_

#include <numeric>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {

// Provides optimizations for mathematical operations having vectors as
// operand(s).
class VectorMath {
 public:
  explicit VectorMath(AvailableCpuFeatures cpu_features)
      : cpu_features_(cpu_features) {}

  // Computes the dot product between two equally sized vectors.
  float DotProductAvx2(rtc::ArrayView<const float> x,
                       rtc::ArrayView<const float> y);
  float DotProduct(rtc::ArrayView<const float> x,
                   rtc::ArrayView<const float> y) {
    RTC_DCHECK_EQ(x.size(), y.size());
    // TODO(bugs.webrtc.org/10480): Add SSE2 alternative implementation.
    // TODO(bugs.webrtc.org/10480): Add NEON alternative implementation.
    return std::inner_product(x.begin(), x.end(), y.begin(), 0.f);
  }

 private:
  AvailableCpuFeatures cpu_features_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_VECTOR_MATH_H_
