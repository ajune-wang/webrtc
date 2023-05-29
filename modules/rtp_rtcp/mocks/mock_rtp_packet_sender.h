/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_MOCKS_MOCK_RTP_PACKET_SENDER_H_
#define MODULES_RTP_RTCP_MOCKS_MOCK_RTP_PACKET_SENDER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "modules/rtp_rtcp/include/rtp_packet_sender.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "test/gmock.h"

namespace webrtc {
class MockRtpPacketSender : public RtpPacketSender {
 public:
  MOCK_METHOD(void,
              EnqueuePackets,
              (std::vector<std::unique_ptr<RtpPacketToSend>>),
              (override));
  MOCK_METHOD(void, RemovePacketsForSsrc, (uint32_t), (override));
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_MOCKS_MOCK_RTP_PACKET_SENDER_H_
