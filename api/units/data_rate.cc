/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/units/data_rate.h"

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

absl::string_view DataRate::ToString(rtc::ArrayView<char> buffer) const {
  if (IsPlusInfinity()) {
    return "+inf bps";
  }

  if (IsMinusInfinity()) {
    return "-inf bps";
  }

  rtc::SimpleStringBuilder sb(buffer);
  if (bps() == 0 || bps() % 1000 != 0) {
    sb << bps() << " bps";
  } else {
    sb << kbps() << " kbps";
  }
  return absl::string_view(sb.str(), sb.size());
}
}  // namespace webrtc
