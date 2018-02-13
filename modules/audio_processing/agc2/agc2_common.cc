/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/agc2_common.h"

#include <cmath>

#include "rtc_base/checks.h"

namespace webrtc {

double DbfsToLinear(const double level) {
  return kInputLevelScaling * std::pow(10.0, level / 20.0);
}

double LinearToDbfs(const double level) {
  if (std::abs(level) <= kInputLevelScaling / 32768.0) {
    return kMinDbfs;
  }
  return 20.0 * std::log10(level / kInputLevelScaling);
}
}  // namespace webrtc
