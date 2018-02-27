/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FPE_OBSERVER_H_
#define TEST_FPE_OBSERVER_H_

#include <cfenv>
#include "test/gtest.h"

namespace webrtc {
namespace test {

// Class that let a unit test fail if floating point exceptions are signaled.
// Usage:
// {
//   FloatingPointExceptionObserver fpe_observer;
//   ...
// }
class FloatingPointExceptionObserver {
 public:
  FloatingPointExceptionObserver(int mask = FE_DIVBYZERO | FE_INVALID |
                                            FE_OVERFLOW | FE_UNDERFLOW)
      : mask_(mask) {
    std::feclearexcept(mask_);
  }
  ~FloatingPointExceptionObserver() {
    EXPECT_FALSE((mask_ & FE_DIVBYZERO) && std::fetestexcept(FE_DIVBYZERO))
        << "Division by zero.";
    EXPECT_FALSE((mask_ & FE_INEXACT) && std::fetestexcept(FE_INEXACT))
        << "Inexact result: rounding during a floating-point operation.";
    EXPECT_FALSE((mask_ & FE_INVALID) && std::fetestexcept(FE_INVALID))
        << "Domain error occurred in an earlier floating-point operation.";
    EXPECT_FALSE((mask_ & FE_OVERFLOW) && std::fetestexcept(FE_OVERFLOW))
        << "The result of a floating-point operation was too large.";
    EXPECT_FALSE((mask_ & FE_UNDERFLOW) && std::fetestexcept(FE_UNDERFLOW))
        << "The result of a floating-point operation was subnormal with a loss "
        << "of precision.";
  }

 private:
  const int mask_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_FPE_OBSERVER_H_
