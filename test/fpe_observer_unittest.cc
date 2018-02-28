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
#include <map>
#include <string>

#include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {

std::map<int, std::string> GetExceptionCodes() {
  return {
      {FE_INVALID, "FE_INVALID"},
// TODO(b/8948): Some floating point exceptions are not signaled on Android.
#ifndef WEBRTC_ANDROID
      {FE_DIVBYZERO, "FE_DIVBYZERO"},
      {FE_OVERFLOW, "FE_OVERFLOW"},
      {FE_UNDERFLOW, "FE_UNDERFLOW"},
#endif
      {FE_INEXACT, "FE_INEXACT"},
  };
}

// Define helper functions as a trick to trigger floating point exceptions at
// run-time.
float GetMinusOne() {
  return -std::cos(0.f);
}

float GetPlusOne() {
  return std::cos(0.f);
}

float GetPlusTwo() {
  return 2.f * std::cos(0.f);
}

// Triggers one or more exception according to the |trigger| mask while
// observing the floating point exceptions defined in the |observe| mask.
void TriggerObserveFloatingPointExceptions(int trigger, int observe) {
  FloatingPointExceptionObserver fpe_observer(observe);
  float tmp = 0.f;
  if (trigger & FE_INVALID)
    tmp = std::sqrt(GetMinusOne());
  if (trigger & FE_DIVBYZERO)
    tmp = 1.f / (GetMinusOne() + GetPlusOne());
  if (trigger & FE_OVERFLOW)
    tmp = std::numeric_limits<float>::max() * GetPlusTwo();
  if (trigger & FE_UNDERFLOW) {
    // TODO(b/8948): Check why FE_UNDERFLOW is not triggered with <float>.
    tmp = std::numeric_limits<double>::min() / GetPlusTwo();
  }
  if (trigger & FE_INEXACT) {
    // TODO(b/8948): Find an expression that triggers FE_INEXACT.
    tmp = 1.f / 3.f;
  }
}

}  // namespace

TEST(FloatingPointExceptionObserver, DISABLED_CheckTestConstants) {
  // Check that the constants used in the test suite behave as expected.
  ASSERT_EQ(0.f, GetMinusOne() + GetPlusOne());
#ifndef WEBRTC_ANDROID
  // Check that all the floating point exceptions are exercised.
  int all_flags = 0;
  for (const auto v : GetExceptionCodes())
    all_flags |= v.first;
  ASSERT_EQ(FE_ALL_EXCEPT, all_flags);
#endif
}

// The floating point exception observer only works in debug mode.
#ifndef NDEBUG
// Trigger each single floating point exception while observing all the other
// exceptions. It must not fail.
TEST(FloatingPointExceptionObserver, CheckNoFalsePositives) {
  for (const auto exception_code : GetExceptionCodes()) {
    SCOPED_TRACE(exception_code.second);
    const int trigger = exception_code.first;
    int observe = FE_ALL_EXCEPT & ~trigger;
    // Over/underflows also trigger FE_INEXACT; hence, ignore FE_INEXACT (which
    // would be a false positive).
    if (trigger & (FE_OVERFLOW | FE_UNDERFLOW))
      observe &= ~FE_INEXACT;
    TriggerObserveFloatingPointExceptions(trigger, observe);
  }
}

// Trigger each single floating point exception while observing it. Check that
// this fails.
TEST(FloatingPointExceptionObserver, CheckNoFalseNegatives) {
  for (const auto exception_code : GetExceptionCodes()) {
    SCOPED_TRACE(exception_code.second);
    const int trigger = exception_code.first;
    // TODO(b/8948): Remove once able to trigger FE_INEXACT.
    if (trigger == FE_INEXACT)
      continue;
    EXPECT_NONFATAL_FAILURE(
        TriggerObserveFloatingPointExceptions(trigger, trigger), "");
  }
}
#endif

}  // namespace test
}  // namespace webrtc
