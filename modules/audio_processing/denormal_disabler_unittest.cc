/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/denormal_disabler.h"

#include <cmath>
#include <limits>

#include "rtc_base/system/arch.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

#if defined(WEBRTC_ARCH_X86_FAMILY)
TEST(DenormalDisabler, EnabledOnX86) {
  DenormalDisabler denormal_disabler;
  EXPECT_TRUE(denormal_disabler.enabled());
}
#endif

#if defined(WEBRTC_ARCH_ARM_FAMILY)
TEST(DenormalDisabler, EnabledOnArm) {
  DenormalDisabler denormal_disabler;
  EXPECT_TRUE(denormal_disabler.enabled());
}
#endif

TEST(DenormalDisabler, ZeroDenormals) {
  DenormalDisabler denormal_disabler;
  if (!denormal_disabler.enabled()) {
    // The current platform does not support `DenormalDisabler`.
    return;
  }
  constexpr float kSmallest = std::numeric_limits<float>::min();
  for (float x : {123.0f, 97.0f, 32.0f, 5.0f, 2.0f}) {
    SCOPED_TRACE(x);
    EXPECT_FLOAT_EQ(kSmallest / x, 0.0f);
  }
}

TEST(DenormalDisabler, InfNotZeroed) {
  DenormalDisabler denormal_disabler;
  if (!denormal_disabler.enabled()) {
    // The current platform does not support `DenormalDisabler`.
    return;
  }
  constexpr float kMax = std::numeric_limits<float>::max();
  for (float x : {-2.0f, 2.0f}) {
    SCOPED_TRACE(x);
    EXPECT_TRUE(std::isinf(kMax * x));
  }
}

TEST(DenormalDisabler, NanNotZeroed) {
  DenormalDisabler denormal_disabler;
  if (!denormal_disabler.enabled()) {
    // The current platform does not support `DenormalDisabler`.
    return;
  }
  const float kNan = std::sqrt(-1.0f);
  EXPECT_TRUE(std::isnan(kNan));
}

}  // namespace
}  // namespace webrtc
