/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_TEST_MOCK_RTP_TRANSPORT_CONTROLLER_SEND_H_
#define CALL_TEST_MOCK_RTP_TRANSPORT_CONTROLLER_SEND_H_

#include <memory>
#include <string>

#include "call/bitrate_constraints.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "modules/congestion_controller/include/network_changed_observer.h"
#include "modules/pacing/packet_router.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/socket.h"
#include "test/gmock.h"

namespace webrtc {
namespace mock_rtp_controller_internal {
// This dummy is because the MOCK macros can't handle noncopyable unique_ptr.
class DummyRtpTransportControllerSend
    : public RtpTransportControllerSendInterface {
  void SetBitrateAllocationStrategy(
      std::unique_ptr<rtc::BitrateAllocationStrategy>
          bitrate_allocation_strategy) override {}
};
}  // namespace  mock_rtp_controller_internal

class MockRtpTransportControllerSend
    : public mock_rtp_controller_internal::DummyRtpTransportControllerSend {
 public:
  MOCK_METHOD0(GetWorkerQueue, rtc::TaskQueue*());
  MOCK_METHOD0(packet_router, PacketRouter*());
  MOCK_METHOD0(transport_feedback_observer, TransportFeedbackObserver*());
  MOCK_METHOD0(packet_sender, RtpPacketSender*());
  MOCK_CONST_METHOD0(keepalive_config, RtpKeepAliveConfig&());
  MOCK_METHOD1(SetPacingFactor, void(float));
  MOCK_METHOD1(SetQueueTimeLimit, void(int));
  MOCK_METHOD0(GetCallStatsObserver, CallStatsObserver*());
  MOCK_METHOD0(GetBitrateAllocator, BitrateAllocator*());
  MOCK_METHOD1(RegisterBitrateAllocationLimitObserver,
               void(BitrateAllocatorLimitObserver*));
  MOCK_METHOD1(RegisterPacketFeedbackObserver, void(PacketFeedbackObserver*));
  MOCK_METHOD1(DeRegisterPacketFeedbackObserver, void(PacketFeedbackObserver*));
  MOCK_METHOD1(RegisterTargetTransferRateObserver,
               void(TargetTransferRateObserver*));
  MOCK_METHOD2(OnNetworkRouteChanged,
               void(const std::string&, const rtc::NetworkRoute&));
  MOCK_METHOD1(OnNetworkAvailability, void(bool));
  MOCK_METHOD0(GetBandwidthObserver, RtcpBandwidthObserver*());
  MOCK_CONST_METHOD0(GetPacerQueuingDelayMs, int64_t());
  MOCK_CONST_METHOD0(GetFirstPacketTimeMs, int64_t());
  MOCK_METHOD1(EnablePeriodicAlrProbing, void(bool));
  MOCK_METHOD1(OnSentPacket, void(const rtc::SentPacket&));
  MOCK_METHOD1(SetSdpBitrateParameters, void(const BitrateConstraints&));
  MOCK_METHOD1(SetClientBitratePreferences,
               void(const BitrateConstraintsMask&));
};
}  // namespace webrtc
#endif  // CALL_TEST_MOCK_RTP_TRANSPORT_CONTROLLER_SEND_H_
