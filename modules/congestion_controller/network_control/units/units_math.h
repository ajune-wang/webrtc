/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_UNITS_MATH_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_UNITS_MATH_H_
#include <stdint.h>
#include <cmath>

namespace webrtc {
namespace units_impl {
inline int64_t DivideAndRound(int64_t numerator, int64_t denominator) {
  return (numerator >= 0) ? (numerator + (denominator / 2)) / denominator
                          : (numerator - (denominator / 2)) / denominator;
}
inline int64_t DoubleToIntRounded(double val) {
  return static_cast<int64_t>(std::round(val));
}
}  // namespace units_impl
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_UNITS_MATH_H_
