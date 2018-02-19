/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_BITRATE_CONFIG_H_
#define CALL_BITRATE_CONFIG_H_

#include <algorithm>

#include "api/optional.h"

namespace webrtc {
struct BitrateConfig {
  static constexpr int kDefaultStartBitrateBps = 300000;
  int min_bitrate_bps = 0;
  int start_bitrate_bps = kDefaultStartBitrateBps;
  int max_bitrate_bps = -1;
};

// The local client's bitrate preferences. The actual configuration used
// is a combination of this and |bitrate_config|. The combination is
// currently more complicated than a simple mask operation (see
// SetBitrateConfig and SetBitrateConfigMask). Assumes that 0 <= min <=
// start <= max holds for set parameters.
struct BitrateConfigMask {
  BitrateConfigMask();
  ~BitrateConfigMask();
  BitrateConfigMask(const BitrateConfigMask&);
  rtc::Optional<int> min_bitrate_bps;
  rtc::Optional<int> start_bitrate_bps;
  rtc::Optional<int> max_bitrate_bps;
};

// Like std::min, but considers non-positive values to be unset.
// TODO(zstein): Remove once all callers use rtc::Optional.
template <typename T>
static T MinPositive(T a, T b) {
  if (a <= 0) {
    return b;
  }
  if (b <= 0) {
    return a;
  }
  return std::min(a, b);
}
}  // namespace webrtc
#endif  // CALL_BITRATE_CONFIG_H_
