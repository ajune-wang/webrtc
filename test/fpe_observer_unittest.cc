/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <iostream>
#include <limits>

#include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {

const uint8_t kExceptionCodes[5] = {FE_INVALID, FE_DIVBYZERO, FE_OVERFLOW,
                                    FE_UNDERFLOW, FE_INEXACT};

// Define non-constexpr values on purpose as a trick to trigger floating point
// exceptions at run-time.
const float kMinusOne = -std::cos(0.f);
const float kPlusOne = std::cos(0.f);
const float kPlusTwo = 2.f * kPlusOne;

// Fix floating point exception flags since some floating point exceptions
// interfere on some platforms.
uint8_t FixObserveMask(uint8_t trigger, uint8_t observe) {
  // Over- and under-flows also trigger FE_INEXACT.
  if (trigger | FE_OVERFLOW || trigger | FE_UNDERFLOW)
    observe =
        (observe | FE_INEXACT) ? observe & ~FE_INEXACT : observe | FE_INEXACT;
  return observe;
}

void TriggerObserveFloatingPointExceptions(uint8_t trigger, uint8_t observe) {
  FloatingPointExceptionObserver fpe_observer(FixObserveMask(trigger, observe));
  float tmp = 0.f;
  if (trigger & FE_INVALID)
    tmp = std::sqrt(kMinusOne);
  if (trigger & FE_DIVBYZERO)
    tmp = 1.f / (kMinusOne + kPlusOne);
  if (trigger & FE_OVERFLOW)
    tmp = std::numeric_limits<float>::max() * kPlusTwo;
  if (trigger & FE_UNDERFLOW) {
    // TODO(b/8948): Check why FE_UNDERFLOW is not triggered with <float>.
    tmp = std::numeric_limits<double>::min() / kPlusTwo;
  }
  if (trigger & FE_INEXACT) {
    // TODO(b/8948): Find an expression that triggers FE_INEXACT.
    tmp = 0.1 + 0.2;
  }
}

}  // namespace

TEST(FloatingPointExceptionObserver, DISABLED_DebugTests) {
  // Check that the constants used in the test suite behave as expected.
  ASSERT_EQ(0.f, kMinusOne + kPlusOne);
  // Check that all the floating point exceptions are exercised.
  uint8_t all_flags = 0u;
  for (const uint8_t v : kExceptionCodes)
    all_flags |= v;
  ASSERT_EQ(FE_ALL_EXCEPT, all_flags);
  // Print exception code values (useful for debugging, these are platform
  // specific).
  std::cout << "FE_INVALID: " << FE_INVALID << std::endl;
  std::cout << "FE_DIVBYZERO: " << FE_DIVBYZERO << std::endl;
  std::cout << "FE_OVERFLOW: " << FE_OVERFLOW << std::endl;
  std::cout << "FE_UNDERFLOW: " << FE_UNDERFLOW << std::endl;
  std::cout << "FE_INEXACT: " << FE_INEXACT << std::endl;
}

// Trigger each single floating point exception while observing all the other
// exceptions. It must not fail.
TEST(FloatingPointExceptionObserver, CheckNoFalsePositives) {
  for (const uint8_t exception_code : kExceptionCodes) {
    SCOPED_TRACE(static_cast<int>(exception_code));
    TriggerObserveFloatingPointExceptions(exception_code,
                                          FE_ALL_EXCEPT & ~exception_code);
  }
}

// Trigger each single floating point exception while observing it. Check that
// this fails.
TEST(FloatingPointExceptionObserver, CheckNoFalseNegatives) {
  for (const uint8_t exception_code : kExceptionCodes) {
    // TODO(b/8948): Remove once able to trigger FE_INEXACT.
    if (exception_code == FE_INEXACT)
      continue;
    SCOPED_TRACE(static_cast<int>(exception_code));
    EXPECT_NONFATAL_FAILURE(
        TriggerObserveFloatingPointExceptions(exception_code, exception_code),
        "");
  }
}

}  // namespace test
}  // namespace webrtc
