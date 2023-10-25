/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/network/received_packet.h"

namespace rtc {

ReceivedPacket::ReceivedPacket(rtc::ArrayView<const uint8_t> payload,
                               webrtc::Timestamp arrival_time)
    : payload_(payload), arrival_time_(arrival_time) {}

}  // namespace rtc
