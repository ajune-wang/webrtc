/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/dtls_stun_piggyback_controller.h"

#include <limits>
#include <memory>
#include <vector>

#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace {
// Extracted from a stock DTLS call using Wireshark.
// Each packet (apart from the last) is truncated to
// the first fragment to keep things short.

// Flight 1 from client to server, containing the Client Hello.
// Sequence number is 6 since this is a resend.
std::vector<uint8_t> dtls_flight1 = {
    0x16, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
    0x8c, 0x01, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0xfe, 0xfd, 0x4b, 0xd6, 0x30, 0xc7, 0x03, 0x0b, 0x56, 0x87, 0x63,
    0x5f, 0x33, 0x3b, 0x04, 0x59, 0xed, 0xe2, 0x6c, 0x36, 0xe4, 0x5d, 0xd6,
    0xe7, 0xd5, 0x80, 0x85, 0xb0, 0x01, 0x39, 0x62, 0x44, 0x82, 0x44, 0x00,
    0x00, 0x00, 0x16, 0xc0, 0x2b, 0xc0, 0x2f, 0xcc, 0xa9, 0xcc, 0xa8, 0xc0,
    0x09, 0xc0, 0x13, 0xc0, 0x0a, 0xc0, 0x14, 0x00, 0x9c, 0x00, 0x2f, 0x00,
    0x35, 0x01, 0x00, 0x00, 0x40, 0x00, 0x17, 0x00, 0x00, 0x00, 0x0a, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x1d, 0x00, 0x17, 0x00, 0x18, 0x00, 0x0d, 0x00,
    0x14, 0x00, 0x12, 0x04, 0x03, 0x08, 0x04, 0x04, 0x01, 0x05, 0x03, 0x08,
    0x05, 0x05, 0x01, 0x08, 0x06, 0x06, 0x01, 0x02, 0x01, 0x00, 0x0b, 0x00,
    0x02, 0x01, 0x00, 0xff, 0x01, 0x00, 0x01, 0x00, 0x00, 0x0e, 0x00, 0x09,
    0x00, 0x06, 0x00, 0x01, 0x00, 0x08, 0x00, 0x07, 0x00};

// Flight 2 from server to client. Server hello.
std::vector<uint8_t> dtls_flight2 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x6c, 0x02, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x60, 0xfe, 0xfd, 0x67, 0x18, 0xfd, 0x2d, 0x99, 0xdb,
    0x75, 0x2a, 0xea, 0x72, 0x1d, 0x04, 0x7e, 0x88, 0x29, 0x06, 0xa8,
    0x93, 0xae, 0xd7, 0xc3, 0xaa, 0xbb, 0x68, 0x9a, 0x1b, 0x1a, 0x74,
    0x23, 0x99, 0x53, 0xf1, 0x20, 0x11, 0x4b, 0xbe, 0x39, 0x14, 0xe6,
    0xab, 0x54, 0x0a, 0xfe, 0x58, 0x9c, 0x3a, 0x46, 0x3a, 0x54, 0xb6,
    0xdb, 0x42, 0xff, 0xe0, 0x6e, 0xe6, 0x1c, 0xb1, 0x11, 0x9b, 0x0f,
    0xab, 0x33, 0x80, 0x92, 0xc0, 0x2b, 0x00, 0x00, 0x18, 0x00, 0x17,
    0x00, 0x00, 0xff, 0x01, 0x00, 0x01, 0x00, 0x00, 0x0b, 0x00, 0x02,
    0x01, 0x00, 0x00, 0x0e, 0x00, 0x05, 0x00, 0x02, 0x00, 0x01, 0x00};

// Flight 3 from client to server. Certificate.
std::vector<uint8_t> dtls_flight3 = {
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x01,
    0x2b, 0x0b, 0x00, 0x01, 0x1f, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x1f, 0x00, 0x01, 0x1c, 0x00, 0x01, 0x19, 0x30, 0x82, 0x01, 0x15, 0x30,
    0x81, 0xbd, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x09, 0x00, 0xed, 0xf3,
    0x7a, 0xa8, 0x8b, 0xa3, 0x72, 0xf3, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86,
    0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x30, 0x11, 0x31, 0x0f, 0x30, 0x0d,
    0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x06, 0x57, 0x65, 0x62, 0x52, 0x54,
    0x43, 0x30, 0x1e, 0x17, 0x0d, 0x32, 0x34, 0x31, 0x30, 0x32, 0x32, 0x31,
    0x33, 0x34, 0x31, 0x33, 0x31, 0x5a, 0x17, 0x0d, 0x32, 0x34, 0x31, 0x31,
    0x32, 0x32, 0x31, 0x33, 0x34, 0x31, 0x33, 0x31, 0x5a, 0x30, 0x11, 0x31,
    0x0f, 0x30, 0x0d, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x06, 0x57, 0x65,
    0x62, 0x52, 0x54, 0x43, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86,
    0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d,
    0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0x4f, 0xa0, 0xbe, 0xdb, 0xc1,
    0x51, 0xf0, 0xe4, 0xe8, 0x76, 0xa9, 0x79, 0xca, 0x2c, 0xda, 0xc8, 0xac,
    0x5b, 0xc6, 0xe8, 0x16, 0x45, 0xe9, 0xb8, 0xa8, 0x44, 0x87, 0x91, 0x5c,
    0xbf, 0x70, 0xbc, 0x0f, 0x11, 0xf6, 0x74, 0xfd, 0x46, 0xe7, 0x97, 0xc8,
    0x30, 0x6a, 0x1b, 0x0b, 0xde, 0x41, 0xf9, 0xf4, 0x3f, 0xc4, 0xf0, 0x9d,
    0x5b, 0x05, 0xf6, 0x4e, 0xd8, 0x30, 0xfa, 0x57, 0xb5, 0x57, 0xd4, 0x30,
    0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x03,
    0x47, 0x00, 0x30, 0x44, 0x02, 0x20, 0x6a, 0xe3, 0xc0, 0x11, 0x57, 0xb0,
    0x3a, 0xec, 0xda, 0x52, 0x09, 0xe6, 0x39, 0xed, 0x27, 0x3a, 0x48, 0xc3,
    0xa1, 0x1e, 0x79, 0x34, 0xd9, 0x9e, 0xf1, 0x32, 0x33, 0x44, 0xd8, 0xdc,
    0xde, 0x2f, 0x02, 0x20, 0x20, 0xbb, 0x1d, 0x16, 0xc7, 0x5e, 0xfb, 0x1d,
    0x86, 0xa6, 0xf4, 0x67, 0x05, 0xb8, 0x9a, 0xa7, 0x7f, 0x20, 0x07, 0x22,
    0x3c, 0xcb, 0xf0, 0x6f, 0xe3, 0x63, 0xdc, 0x9c, 0xa4, 0x70, 0xdb, 0xf4};

// Flight four from server to client.
// Change Cipher spec, Encrypted handshake message.
std::vector<uint8_t> dtls_flight4 = {
    0x14, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0x00, 0x01, 0x01, 0x16, 0xfe, 0xfd, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xba, 0xd0, 0x2a, 0xda, 0x76, 0xd3, 0x0e, 0xbf, 0x89,
    0x2e, 0x57, 0x18, 0x85, 0x31, 0x02, 0x19, 0xc1, 0xde, 0x51, 0x31,
    0x98, 0x13, 0x76, 0x95, 0x40, 0x5d, 0x71, 0x08, 0xae, 0xe7, 0xea,
    0x02, 0x63, 0x8e, 0xf2, 0x50, 0xb3, 0xd5, 0x08, 0x01};

std::vector<uint8_t> empty = {};
}  // namespace

namespace cricket {

using State = DtlsStunPiggybackController::State;

class DtlsStunPiggybackControllerTest : public ::testing::Test {
 protected:
  DtlsStunPiggybackControllerTest()
      : client_([](rtc::ArrayView<const uint8_t> data) {}),
        server_([](rtc::ArrayView<const uint8_t> data) {}) {}

  void SendClientToServer(const std::vector<uint8_t> data,
                          StunMessageType type) {
    client_.SetDataToPiggyback(data);
    std::unique_ptr<StunByteStringAttribute> attr_data;
    if (client_.GetDataToPiggyback(type)) {
      attr_data = std::make_unique<StunByteStringAttribute>(
          STUN_ATTR_META_DTLS_IN_STUN, *client_.GetDataToPiggyback(type));
    }
    std::unique_ptr<StunUInt64Attribute> attr_ack;
    if (client_.GetAckToPiggyback(type)) {
      attr_ack = std::make_unique<StunUInt64Attribute>(
          STUN_ATTR_META_DTLS_IN_STUN_ACK, *client_.GetAckToPiggyback(type));
    }
    server_.ReportDataPiggybacked(attr_data.get(), attr_ack.get());
    if (data == dtls_flight3) {
      // When receiving flight 3, server handshake is complete.
      server_.SetDtlsHandshakeComplete(/*is_client=*/false);
    }
  }
  void SendServerToClient(const std::vector<uint8_t> data,
                          StunMessageType type) {
    server_.SetDataToPiggyback(data);
    std::unique_ptr<StunByteStringAttribute> attr_data;
    if (server_.GetDataToPiggyback(type)) {
      attr_data = std::make_unique<StunByteStringAttribute>(
          STUN_ATTR_META_DTLS_IN_STUN, *server_.GetDataToPiggyback(type));
    }
    std::unique_ptr<StunUInt64Attribute> attr_ack;
    if (server_.GetAckToPiggyback(type)) {
      attr_ack = std::make_unique<StunUInt64Attribute>(
          STUN_ATTR_META_DTLS_IN_STUN_ACK, *server_.GetAckToPiggyback(type));
    }
    client_.ReportDataPiggybacked(attr_data.get(), attr_ack.get());
    if (data == dtls_flight4) {
      // When receiving flight 4, client handshake is complete.
      client_.SetDtlsHandshakeComplete(/*is_client=*/true);
    }
  }

  void DisableSupport(DtlsStunPiggybackController& client_or_server) {
    ASSERT_EQ(client_or_server.state(), State::TENTATIVE);
    client_or_server.ReportDataPiggybacked(nullptr, nullptr);
    ASSERT_EQ(client_or_server.state(), State::OFF);
  }

  DtlsStunPiggybackController client_;
  DtlsStunPiggybackController server_;
};

TEST_F(DtlsStunPiggybackControllerTest, BasicHandshake) {
  // Flight 1+2
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  SendServerToClient(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest, FirstClientPacketLost) {
  // Client to server got lost (or arrives late)
  // Flight 1
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(dtls_flight1, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 2+3
  SendServerToClient(dtls_flight2, STUN_BINDING_REQUEST);
  SendClientToServer(dtls_flight3, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 4
  SendServerToClient(dtls_flight4, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest, NotSupportedByServer) {
  DisableSupport(server_);

  // Flight 1
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClient(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, NotSupportedByServerClientReceives) {
  DisableSupport(server_);

  // Client to server got lost (or arrives late)
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  EXPECT_EQ(client_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, NotSupportedByClient) {
  DisableSupport(client_);

  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, SomeRequestsDoNotGoThrough) {
  // Client to server got lost (or arrives late)
  // Flight 1
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(dtls_flight1, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 1+2, server sent request got lost.
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK
  SendClientToServer(empty, STUN_BINDING_REQUEST);
  SendServerToClient(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest, LossOnPostHandshakeAck) {
  // Flight 1+2
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.state(), State::CONFIRMED);
  SendServerToClient(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(client_.state(), State::CONFIRMED);

  // Flight 3+4
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::PENDING);
  EXPECT_EQ(client_.state(), State::PENDING);

  // Post-handshake ACK. Client to server gets lost
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
}

TEST_F(DtlsStunPiggybackControllerTest,
       UnsupportedStateAfterFallbackHandshakeRemainsOff) {
  DisableSupport(client_);
  DisableSupport(server_);

  // Set DTLS complete after normal handshake.
  client_.SetDtlsHandshakeComplete(true);
  EXPECT_EQ(client_.state(), State::OFF);
  server_.SetDtlsHandshakeComplete(true);
  EXPECT_EQ(server_.state(), State::OFF);
}

TEST_F(DtlsStunPiggybackControllerTest, BasicHandshakeAckData) {
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_REQUEST),
            std::numeric_limits<uint64_t>::max());
  // Flight 1+2
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight2, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE), 6);
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_REQUEST), 0);

  // Flight 3+4
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  SendServerToClient(dtls_flight4, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE), 7);
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_REQUEST), 0x01000000000000);

  // Post-handshake ACK
  SendServerToClient(empty, STUN_BINDING_REQUEST);
  SendClientToServer(empty, STUN_BINDING_RESPONSE);
  EXPECT_EQ(server_.state(), State::COMPLETE);
  EXPECT_EQ(client_.state(), State::COMPLETE);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE), std::nullopt);
  EXPECT_EQ(client_.GetAckToPiggyback(STUN_BINDING_REQUEST), std::nullopt);
}

TEST_F(DtlsStunPiggybackControllerTest, HighestAckData) {
  // Flight 1+2
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST), 6);
  SendClientToServer(dtls_flight3, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST), 7);

  // Receive Flight 1 again
  SendClientToServer(dtls_flight1, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_REQUEST), 7);
}

TEST_F(DtlsStunPiggybackControllerTest, InvalidDtlsPackets) {
  std::vector<uint8_t> length_invalid = {0x16, 0xfe, 0xff, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x12, 0x34};
  SendClientToServer(length_invalid, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::numeric_limits<uint64_t>::max());

  std::vector<uint8_t> too_short = {0x00};
  SendClientToServer(too_short, STUN_BINDING_REQUEST);
  EXPECT_EQ(server_.GetAckToPiggyback(STUN_BINDING_RESPONSE),
            std::numeric_limits<uint64_t>::max());
}

}  // namespace cricket
