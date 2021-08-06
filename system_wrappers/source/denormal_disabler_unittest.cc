/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/denormal_disabler.h"

#include <cmath>
#include <limits>
#include <vector>

#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

// Not using constexpr to avoid compile-time resolution of expressions.
const float kSmallest = std::numeric_limits<float>::min();

// Returns a number of float values such that, if used as divisors of
// `kSmallest`, the division produces a denormal or zero depending on whether
// denormals are enabled.
std::vector<float> GetDenormalDivisors() {
  return {123.125f, 97.0f, 32.0f, 5.0f, 1.5f};
}

// Returns true if the result of `dividend` / `divisor` is a denormal.
// `dividend` and `divisor` must not be denormals.
bool CheckDenormalDivision(float dividend, float divisor) {
  RTC_DCHECK_GE(std::fabsf(dividend), kSmallest);
  RTC_DCHECK_GE(std::fabsf(divisor), kSmallest);
  float division = dividend / divisor;
  return division > 0.0f || division < 0.0f;
}

}  // namespace

// Checks that `DenormalDisabler` can be disabled regardless of whether
// architecture and compiler are supported.
TEST(DenormalDisabler, Disable) {
  DenormalDisabler denormal_disabler(/*enabled=*/false);
  EXPECT_FALSE(denormal_disabler.enabled());
}

class DenormalDisablerParametrization : public ::testing::TestWithParam<bool> {
};

// Checks that +inf and -inf are not zeroed regardless of whether
// architecture and compiler are supported.
TEST_P(DenormalDisablerParametrization, InfNotZeroed) {
  DenormalDisabler denormal_disabler(/*enabled=*/GetParam());
  constexpr float kMax = std::numeric_limits<float>::max();
  for (float x : {-2.0f, 2.0f}) {
    SCOPED_TRACE(x);
    EXPECT_TRUE(std::isinf(kMax * x));
  }
}

// Checks that a NaN is not zeroed regardless of whether architecture and
// compiler are supported.
TEST_P(DenormalDisablerParametrization, NanNotZeroed) {
  DenormalDisabler denormal_disabler(/*enabled=*/GetParam());
  const float kNan = std::sqrt(-1.0f);
  EXPECT_TRUE(std::isnan(kNan));
}

INSTANTIATE_TEST_SUITE_P(DenormalDisabler,
                         DenormalDisablerParametrization,
                         ::testing::Values(false, true),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "enabled" : "disabled";
                         });

#if defined(WEBRTC_DENORMAL_DISABLER_SUPPORTED)

// Checks that denormals are not zeroed if `DenormalDisabler` is disabled and
// architecture and compiler are supported.
TEST(DenormalDisabler, DoNotZeroDenormalsIfDisabled) {
  // Read a denormal divisor from a vector to avoid compile-time divisions.
  const float denormal_divisor = GetDenormalDivisors().front();

  // Force the CPU to use denormals.
  ASSERT_TRUE(CheckDenormalDivision(kSmallest, denormal_divisor))
      << "Precondition not met: denormals must be enabled.";

  DenormalDisabler denormal_disabler(/*enabled=*/false);
  for (float x : GetDenormalDivisors()) {
    SCOPED_TRACE(x);
    EXPECT_TRUE(CheckDenormalDivision(-kSmallest, x));
    EXPECT_TRUE(CheckDenormalDivision(kSmallest, x));
  }
}

// Checks that `DenormalDisabler` can be enabled if architecture and compiler
// are supported.
TEST(DenormalDisabler, Enable) {
  DenormalDisabler denormal_disabler(/*enabled=*/true);
  EXPECT_TRUE(denormal_disabler.enabled());
}

// Checks that denormals are zeroed if `DenormalDisabler` is enabled.
TEST(DenormalDisabler, ZeroDenormals) {
  DenormalDisabler denormal_disabler(/*enabled=*/true);
  for (float x : GetDenormalDivisors()) {
    SCOPED_TRACE(x);
    EXPECT_FALSE(CheckDenormalDivision(-kSmallest, x));
    EXPECT_FALSE(CheckDenormalDivision(kSmallest, x));
  }
}

// Checks that the `DenormalDisabler` dtor re-enables denormals if previously
// enabled.
TEST(DenormalDisabler, RestoreDenormalsEnabled) {
  // Read a denormal divisor from a vector to avoid compile-time divisions.
  const float denormal_divisor = GetDenormalDivisors().front();

  ASSERT_TRUE(CheckDenormalDivision(kSmallest, denormal_divisor))
      << "Precondition not met: denormals must be enabled.";
  {
    DenormalDisabler denormal_disabler(/*enabled=*/true);
    ASSERT_FALSE(CheckDenormalDivision(kSmallest, denormal_divisor));
  }
  EXPECT_TRUE(CheckDenormalDivision(kSmallest, denormal_divisor));
}

// Checks that the `DenormalDisabler` dtor keeps denormals disabled if
// previously disabled - i.e., nested usage is supported.
TEST(DenormalDisabler, ZeroDenormalsNested) {
  // Read a denormal divisor from a vector to avoid compile-time divisions.
  const float denormal_divisor = GetDenormalDivisors().front();

  DenormalDisabler d1(/*enabled=*/true);
  ASSERT_FALSE(CheckDenormalDivision(kSmallest, denormal_divisor));
  {
    DenormalDisabler d2(/*enabled=*/true);
    ASSERT_FALSE(CheckDenormalDivision(kSmallest, denormal_divisor));
  }
  EXPECT_FALSE(CheckDenormalDivision(kSmallest, denormal_divisor));
}

#else  // !defined(WEBRTC_DENORMAL_DISABLER_SUPPORTED)

// Checks that `DenormalDisabler` cannot be enabled if architecture and
// compiler are not supported.
TEST(DenormalDisabler, CannotEnable) {
  DenormalDisabler denormal_disabler(/*enabled=*/true);
  EXPECT_FALSE(denormal_disabler.enabled());
}

// Checks that `DenormalDisabler` does not zero denormals if architecture and
// compiler are not supported.
TEST(DenormalDisabler, DoNotZeroDenormalsIfUnsupported) {
  DenormalDisabler denormal_disabler(/*enabled=*/true);
  for (float x : GetDenormalDivisors()) {
    SCOPED_TRACE(x);
    EXPECT_FALSE(CheckDenormalDivision(-kSmallest, x));
    EXPECT_FALSE(CheckDenormalDivision(kSmallest, x));
  }
}

#endif

}  // namespace webrtc
