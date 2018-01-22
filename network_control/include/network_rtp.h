/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_CONTROL_INCLUDE_NETWORK_RTP_H_
#define NETWORK_CONTROL_INCLUDE_NETWORK_RTP_H_
#include <stdint.h>
#include <vector>
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "network_control/include/network_types.h"

namespace webrtc {

NetworkPacketFeedback NetworkPacketFeedbackFromRtpPacketFeedback(
    const webrtc::PacketFeedback&);

TransportPacketsFeedback TransportPacketsFeedbackFromRtpFeedbackVector(
    const std::vector<PacketFeedback>&,
    int64_t creation_time_ms);

TransportPacketsFeedback TransportPacketsFeedbackFromRtpFeedbackVector(
    const std::vector<PacketFeedback>&);

}  // namespace webrtc

#endif  // NETWORK_CONTROL_INCLUDE_NETWORK_RTP_H_
