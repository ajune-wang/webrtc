/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_AGC2_TESTING_COMMON_H_
#define MODULES_AUDIO_PROCESSING_AGC2_AGC2_TESTING_COMMON_H_

#include <vector>

#include "rtc_base/basictypes.h"

namespace webrtc {

// Limiter params.
constexpr double kLimiterMaxInputLevel = 1.0;
constexpr double kLimiterKneeSmoothness = 1.0;
constexpr double kLimiterCompressionRatio = 5.0;

std::vector<double> LinSpace(const double l, const double r, size_t num_points);
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_AGC2_TESTING_COMMON_H_
