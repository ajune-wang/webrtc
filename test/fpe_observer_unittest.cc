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
#include <limits>

#include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {

const uint8_t kExceptionCodes[5] = {
    FE_INVALID,    // 1
    FE_DIVBYZERO,  // 4
    FE_OVERFLOW,   // 8
    FE_UNDERFLOW,  // 16
    FE_INEXACT     // 32
};

// Define non-constexpr values on purpose as a trick to trigger floating point
// exceptions at run-time.
const float kMinusOne = -std::cos(0.f);
const float kPlusOne = std::cos(0.f);
const float kMoreThanOne = 3 * kPlusOne;

void TriggerObserveFloatingPointExceptions(uint8_t trigger, uint8_t observe) {
  // Fix observe since some floating point exceptions cannot be isolated.
  // TODO(alessiob): Fix observe.

  FloatingPointExceptionObserver fpe_observer(observe);
  float tmp = 0.f;
  if (trigger & FE_INVALID)
    tmp = std::sqrt(kMinusOne);  // Out of domain.
  if (trigger & FE_DIVBYZERO)
    tmp = 1.f / (kMinusOne + kPlusOne);
  if (trigger & FE_OVERFLOW)
    tmp = std::numeric_limits<float>::max() * kMoreThanOne;
  if (trigger & FE_UNDERFLOW)
    tmp = std::numeric_limits<float>::min() / kMoreThanOne;
  if (trigger & FE_INEXACT)
    tmp = 1.f / 3.f;  // No exact representation in floating point.
}

}  // namespace

TEST(FloatingPointExceptionObserver, CheckObserver) {
  ASSERT_EQ(0.f, kMinusOne + kPlusOne);
  // Trigger each single floating point exception while observing all the other
  // exceptions. It must not fail.
  for (const uint8_t exception_code : kExceptionCodes) {
    SCOPED_TRACE(static_cast<int>(exception_code));
    uint8_t mask = FE_ALL_EXCEPT & ~exception_code;
    TriggerObserveFloatingPointExceptions(exception_code, mask);
  }
  // Trigger each single floating point exception while observing it. Check that
  // this fails.
  for (const uint8_t exception_code : kExceptionCodes) {
    SCOPED_TRACE(static_cast<int>(exception_code));
    EXPECT_NONFATAL_FAILURE(TriggerObserveFloatingPointExceptions(
        exception_code, exception_code), "");
  }
}

}  // namespace test
}  // namespace webrtc
