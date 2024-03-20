/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_transport_controller_send.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "api/environment/environment_factory.h"
#include "call/rtp_transport_config.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

TEST(RtpTransportControllerSendTest,
     AllocatesTransportSequenceNumbersIfHeaderExtensionReserved) {
  const uint16_t kSsrc = 1234;
  uint16_t expected_transport_sequence_number = 0xFFF0;
  RtpTransportConfig config{
      .env = EnvironmentFactory().Create(),
      .start_transport_sequence_number = expected_transport_sequence_number};
  RtpTransportControllerSend controller_send(config);
  PacketRouter* packet_router = controller_send.packet_router();
  ASSERT_NE(packet_router, nullptr);
  NiceMock<MockRtpRtcpInterface> rtp;

  ON_CALL(rtp, SSRC()).WillByDefault(Return(kSsrc));
  ON_CALL(rtp, SendingMedia()).WillByDefault(Return(true));
  packet_router->AddSendRtpModule(&rtp, /*remb_candidate=*/false);

  RtpHeaderExtensionMap extension_manager;
  extension_manager.Register<TransportSequenceNumber>(/*id=*/1);

  for (size_t i = 0; i < 20; ++i) {
    auto packet = std::make_unique<RtpPacketToSend>(&extension_manager);
    packet->SetSsrc(kSsrc);
    packet->set_packet_type(RtpPacketMediaType::kAudio);
    packet->ReserveExtension<TransportSequenceNumber>();
    EXPECT_CALL(rtp, TrySendPacket)
        .WillRepeatedly([&](std::unique_ptr<RtpPacketToSend> packet,
                            const PacedPacketInfo& pacing_info) {
          absl::optional<uint16_t> transport_sequence_number =
              packet->GetExtension<TransportSequenceNumber>();
          EXPECT_EQ(transport_sequence_number,
                    expected_transport_sequence_number);
          ++expected_transport_sequence_number;
          return true;
        });
    packet_router->SendPacket(std::move(packet), PacedPacketInfo());
  }
  packet_router->OnBatchComplete();
  packet_router->RemoveSendRtpModule(&rtp);
}

TEST(RtpTransportControllerSendTest,
     DoNotAllocateTransportSequenceIfHeaderExtensionNotReserved) {
  const uint16_t kSsrc = 1234;
  RtpTransportConfig config{.env = EnvironmentFactory().Create()};
  RtpTransportControllerSend controller_send(config);
  PacketRouter* packet_router = controller_send.packet_router();
  ASSERT_NE(packet_router, nullptr);
  NiceMock<MockRtpRtcpInterface> rtp;

  ON_CALL(rtp, SSRC()).WillByDefault(Return(kSsrc));
  ON_CALL(rtp, SendingMedia()).WillByDefault(Return(true));
  packet_router->AddSendRtpModule(&rtp, /*remb_candidate=*/false);

  RtpHeaderExtensionMap extension_manager;
  for (size_t i = 0; i < 20; ++i) {
    auto packet = std::make_unique<RtpPacketToSend>(&extension_manager);
    packet->SetSsrc(kSsrc);
    packet->set_packet_type(RtpPacketMediaType::kAudio);
    EXPECT_CALL(rtp, TrySendPacket)
        .WillRepeatedly([&](std::unique_ptr<RtpPacketToSend> packet,
                            const PacedPacketInfo& pacing_info) {
          absl::optional<uint16_t> transport_sequence_number =
              packet->GetExtension<TransportSequenceNumber>();
          // Since the packet has not reserved space for transport sequence
          // number before SendPacket, it is not set when moved to the RTP
          // module either.
          EXPECT_FALSE(transport_sequence_number.has_value());
          return true;
        });
    packet_router->SendPacket(std::move(packet), PacedPacketInfo());
  }
  packet_router->OnBatchComplete();
  packet_router->RemoveSendRtpModule(&rtp);
}

}  // namespace
}  // namespace webrtc
