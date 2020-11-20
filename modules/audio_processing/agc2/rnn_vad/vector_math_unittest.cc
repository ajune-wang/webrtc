/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/vector_math.h"

#include "modules/audio_processing/agc2/cpu_features.h"
#include "rtc_base/system/arch.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr int kSizeOfX = 19;
constexpr float kX[kSizeOfX] = {
    0.31593041f, 0.9350786f,   -0.25252445f, -0.86956251f, -0.9673632f,
    0.54571901f, -0.72504495f, -0.79509912f, -0.25525012f, -0.73340473f,
    0.15747377f, -0.04370565f, 0.76135145f,  -0.57239645f, 0.68616848f,
    0.3740298f,  0.34710799f,  -0.92207423f, 0.10738454f};
constexpr int kSizeOfXSubSpan = 16;
static_assert(kSizeOfXSubSpan < kSizeOfX, "");
constexpr float kEnergyOfX = 7.315563958160327f;
constexpr float kEnergyOfXSubspan = 6.333327669592963f;

TEST(RnnVadTest, TestDotProduct) {
  VectorMath vector_math(/*cpu_features=*/{});  // No optimizations.
  EXPECT_FLOAT_EQ(vector_math.DotProduct(kX, kX), kEnergyOfX);
  EXPECT_FLOAT_EQ(
      vector_math.DotProduct({kX, kSizeOfXSubSpan}, {kX, kSizeOfXSubSpan}),
      kEnergyOfXSubspan);
}

#if defined(WEBRTC_ARCH_X86_FAMILY)

TEST(RnnVadTest, TestDotProductAvx2) {
  const AvailableCpuFeatures cpu_features = GetAvailableCpuFeatures();
  if (!cpu_features.avx2) {
    return;
  }
  VectorMath vector_math(cpu_features);
  EXPECT_FLOAT_EQ(vector_math.DotProductAvx2(kX, kX), kEnergyOfX);
  EXPECT_FLOAT_EQ(
      vector_math.DotProductAvx2({kX, kSizeOfXSubSpan}, {kX, kSizeOfXSubSpan}),
      kEnergyOfXSubspan);
}

#endif  // WEBRTC_ARCH_X86_FAMILY

}  // namespace
}  // namespace rnn_vad
}  // namespace webrtc
