/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/media/media_transport_config.h"

#include "absl/strings/str_format.h"
#include "rtc_base/checks.h"

namespace webrtc {

MediaTransportConfig::MediaTransportConfig(size_t rtp_max_packet_size)
    : rtp_max_packet_size(rtp_max_packet_size) {
  RTC_DCHECK_GT(rtp_max_packet_size, 0);
}

std::string MediaTransportConfig::DebugString() const {
  return absl::StrFormat("{rtp_max_packet_size: %u}",
                         rtp_max_packet_size.value_or(0));
}

}  // namespace webrtc
