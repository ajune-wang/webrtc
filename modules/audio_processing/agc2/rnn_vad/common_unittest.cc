/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/common.h"

#include <limits>

#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr uint32_t kAllOptimizationsMask = std::numeric_limits<uint32_t>::max();

TEST(RnnVadTest, NoOptimizationIfAllUnsupported) {
  EXPECT_EQ(GetBestOptimization(/*supported_mask=*/0,
                                /*disabled_mask=*/0),
            Optimization::kNone);
}

TEST(RnnVadTest, NoOptimizationIfAllDisabled) {
  EXPECT_EQ(GetBestOptimization(
                /*supported_mask=*/kAllOptimizationsMask,
                /*disabled_mask=*/kAllOptimizationsMask),
            Optimization::kNone);
}

#if defined(WEBRTC_ARCH_X86_FAMILY)

TEST(RnnVadTest, PreferAvx2OverSse2) {
  if (!IsOptimizationAvailable(Optimization::kAvx2) ||
      !IsOptimizationAvailable(Optimization::kSse2)) {
    return;
  }
  EXPECT_EQ(GetBestOptimization(
                /*supported_mask=*/Optimization::kAvx2 | Optimization::kSse2,
                /*disabled_mask=*/0),
            Optimization::kAvx2);
}

TEST(RnnVadTest, FallBackToSse2IfAvx2IsDisabled) {
  if (!IsOptimizationAvailable(Optimization::kAvx2) ||
      !IsOptimizationAvailable(Optimization::kSse2)) {
    return;
  }
  EXPECT_EQ(GetBestOptimization(
                /*supported_mask=*/Optimization::kAvx2 | Optimization::kSse2,
                /*disabled_mask=*/Optimization::kAvx2),
            Optimization::kSse2);
}

TEST(RnnVadTest, FallBackToSse2IfAvx2IsUnsupported) {
  if (!IsOptimizationAvailable(Optimization::kAvx2) ||
      !IsOptimizationAvailable(Optimization::kSse2)) {
    return;
  }
  EXPECT_EQ(GetBestOptimization(
                /*supported_mask=*/Optimization::kSse2,
                /*disabled_mask=*/0),
            Optimization::kSse2);
}

TEST(RnnVadTest, PreferSse2OverNone) {
  if (!IsOptimizationAvailable(Optimization::kSse2)) {
    return;
  }
  EXPECT_EQ(GetBestOptimization(
                /*supported_mask=*/Optimization::kSse2,
                /*disabled_mask=*/0),
            Optimization::kSse2);
}

#endif  // WEBRTC_ARCH_X86_FAMILY

#if defined(WEBRTC_HAS_NEON)

TEST(RnnVadTest, PreferNeonOverNone) {
  if (!IsOptimizationAvailable(Optimization::kNeon)) {
    return;
  }
  EXPECT_EQ(GetBestOptimization(
                /*supported_mask=*/Optimization::kNeon,
                /*disabled_mask=*/0),
            Optimization::kNeon);
}

#endif  // WEBRTC_HAS_NEON

}  // namespace
}  // namespace rnn_vad
}  // namespace webrtc
