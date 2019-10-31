/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/comfort_noise_generator.h"

#include <algorithm>
#include <numeric>

#include "modules/audio_processing/aec3/aec_state.h"
#include "rtc_base/random.h"
#include "rtc_base/system/arch.h"
#include "system_wrappers/include/cpu_features_wrapper.h"
#include "test/gtest.h"

namespace webrtc {
namespace aec3 {
namespace {

float Power(const FftData& N) {
  std::array<float, kFftLengthBy2Plus1> N2;
  N.Spectrum(Aec3Optimization::kNone, N2);
  return std::accumulate(N2.begin(), N2.end(), 0.f) / N2.size();
}

}  // namespace

TEST(ComfortNoiseGenerator, CorrectLevel) {
  ComfortNoiseGenerator cng(DetectOptimization(), 1);
  AecState aec_state(EchoCanceller3Config{}, 1);

  std::vector<std::array<float, kFftLengthBy2Plus1>> N2(1);
  N2[0].fill(1000.f * 1000.f);

  std::vector<FftData> n_lower(1);
  std::vector<FftData> n_upper(1);
  n_lower[0].re.fill(0.f);
  n_lower[0].im.fill(0.f);
  n_upper[0].re.fill(0.f);
  n_upper[0].im.fill(0.f);

  // Ensure instantaneous updata to nonzero noise.
  cng.Compute(false, N2, n_lower, n_upper);

  EXPECT_LT(0.f, Power(n_lower[0]));
  EXPECT_LT(0.f, Power(n_upper[0]));

  for (int k = 0; k < 10000; ++k) {
    cng.Compute(false, N2, n_lower, n_upper);
  }
  EXPECT_NEAR(2.f * N2[0][0], Power(n_lower[0]), N2[0][0] / 10.f);
  EXPECT_NEAR(2.f * N2[0][0], Power(n_upper[0]), N2[0][0] / 10.f);
}

}  // namespace aec3
}  // namespace webrtc
