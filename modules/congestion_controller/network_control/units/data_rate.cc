/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/network_control/units/data_rate.h"
#include <cmath>

namespace webrtc {
DataRate DataRate::operator*(double scalar) const {
  return DataRate::bytes_per_second(std::round(bytes_per_second() * scalar));
}
}  // namespace webrtc
