/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_TEST_MOCK_RTP_SEND_TRANSPORT_CONTROLLER_H_
#define CALL_TEST_MOCK_RTP_SEND_TRANSPORT_CONTROLLER_H_

#include <string>

#include "call/rtp_send_transport_controller_interface.h"
#include "test/gmock.h"

namespace webrtc {

class MockRtpSendTransportController
    : public RtpSendTransportControllerInterface {
 public:
  MOCK_METHOD0(packet_router, PacketRouter*());
  MOCK_METHOD0(transport_feedback_observer, TransportFeedbackObserver*());
  MOCK_METHOD0(packet_sender, RtpPacketSender*());
  MOCK_CONST_METHOD0(keepalive_config, RtpKeepAliveConfig&());
  MOCK_METHOD2(SetAllocatedSendBitrateLimits, void(int, int));
  MOCK_METHOD0(GetPacerModule, Module*());
  MOCK_METHOD1(SetPacingFactor, void(float));
  MOCK_METHOD1(SetQueueTimeLimit, void(int));
  MOCK_METHOD0(GetModule, Module*());
  MOCK_METHOD0(GetCallStatsObserver, CallStatsObserver*());
  MOCK_METHOD1(RegisterPacketFeedbackObserver, void(PacketFeedbackObserver*));
  MOCK_METHOD1(DeRegisterPacketFeedbackObserver, void(PacketFeedbackObserver*));
  MOCK_METHOD1(RegisterNetworkObserver, void(NetworkChangedObserver*));
  MOCK_METHOD1(DeRegisterNetworkObserver, void(NetworkChangedObserver*));
  MOCK_METHOD2(OnNetworkRouteChanged,
               void(const std::string&, const rtc::NetworkRoute&));
  MOCK_METHOD1(OnNetworkAvailability, void(bool));
  MOCK_METHOD1(SetTransportOverhead, void(size_t));
  MOCK_METHOD0(GetBandwidthObserver, RtcpBandwidthObserver*());
  MOCK_CONST_METHOD1(AvailableBandwidth, bool(uint32_t*));
  MOCK_CONST_METHOD0(GetPacerQueuingDelayMs, int64_t());
  MOCK_CONST_METHOD0(GetFirstPacketTimeMs, int64_t());
  MOCK_METHOD0(GetRetransmissionRateLimiter, RateLimiter*());
  MOCK_METHOD1(EnablePeriodicAlrProbing, void(bool));
  MOCK_METHOD1(OnSentPacket, void(const rtc::SentPacket&));
  MOCK_METHOD1(SetBitrateConfig, void(const BitrateConfig&));
  MOCK_METHOD1(SetBitrateConfigMask, void(const BitrateConfigMask&));
};
}  // namespace webrtc
#endif  // CALL_TEST_MOCK_RTP_SEND_TRANSPORT_CONTROLLER_H_
