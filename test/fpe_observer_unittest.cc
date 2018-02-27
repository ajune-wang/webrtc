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
#include <ostream>

#include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {

const int kExceptionCodes[5] = {FE_DIVBYZERO, FE_INEXACT, FE_INVALID,
                                FE_OVERFLOW, FE_UNDERFLOW};

// The values below are not declared constexpr on purpose. Doing that
// would trigger compile time warnings that are not relevant for this
// file.
const float kZero = 0.f;
const float kOne = 1.f;
const float kMinFloat = std::numeric_limits<float>::min();
const float kMaxFloat = std::numeric_limits<float>::max();

void TriggerFloatingPointExceptions(int exception_mask) {
  float tmp = 0.f;
  if (exception_mask & FE_DIVBYZERO)
    tmp = kOne / kZero;
  if (exception_mask & FE_INEXACT)
    tmp = 0.1f + 0.2f;  // No exact representation in floating point.
  if (exception_mask & FE_INVALID)
    tmp = std::sqrt(-1.f);  // Out of domain.
  if (exception_mask & FE_OVERFLOW)
    tmp = kMaxFloat * 3;
  if (exception_mask & FE_UNDERFLOW)
    tmp = kMinFloat / 3;
}

// Observe while triggering foating point exceptions (one-by-one).
void TriggerFloatingPointExceptionObserverFailures() {
  for (const auto mask : kExceptionCodes) {
    std::ostringstream ss;
    ss << "floating point exception code: " << mask;
    // SCOPED_TRACE(ss.str());
    FloatingPointExceptionObserver fpe_observer(mask);
    TriggerFloatingPointExceptions(mask);
  }
}

}  // namespace

TEST(FloatingPointExceptionObserver, CheckObserver) {
  EXPECT_NONFATAL_FAILURE(TriggerFloatingPointExceptionObserverFailures(), "");
  // Trigger floating point exceptions while observing all the others.
  for (const auto exclude : kExceptionCodes) {
    std::ostringstream ss;
    ss << "floating point exception code: " << exclude;
    SCOPED_TRACE(ss.str());
    const int mask = FE_ALL_EXCEPT & !exclude;
    FloatingPointExceptionObserver fpe_observer(mask);
    TriggerFloatingPointExceptions(mask);
  }
}

}  // namespace test
}  // namespace webrtc
