/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/socket/packet_sender.h"

#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/packet/chunk/cookie_ack_chunk.h"
#include "net/dcsctp/socket/mock_dcsctp_socket_callbacks.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::_;

constexpr VerificationTag kVerificationTag(123);

class PacketSenderTest : public testing::Test {
 protected:
  PacketSenderTest()
      : timer_manager_([this]() { return callbacks_.CreateTimeout(); }),
        sender_(timer_manager_, callbacks_, on_send_fn_.AsStdFunction()) {}

  SctpPacket::Builder PacketBuilder() const {
    return SctpPacket::Builder(kVerificationTag, options_);
  }

  void RunTimers() {
    for (;;) {
      absl::optional<TimeoutID> timeout_id = callbacks_.GetNextExpiredTimeout();
      if (!timeout_id.has_value()) {
        break;
      }
      timer_manager_.HandleTimeout(*timeout_id);
    }
  }

  DcSctpOptions options_;
  testing::NiceMock<MockDcSctpSocketCallbacks> callbacks_;
  testing::MockFunction<void(rtc::ArrayView<const uint8_t>, SendPacketStatus)>
      on_send_fn_;
  TimerManager timer_manager_;
  PacketSender sender_;
};

TEST_F(PacketSenderTest, SendPacketCallsCallback) {
  EXPECT_CALL(on_send_fn_, Call(_, SendPacketStatus::kSuccess));
  EXPECT_TRUE(sender_.Send(PacketBuilder().Add(CookieAckChunk())));

  EXPECT_CALL(callbacks_, SendPacketWithStatus)
      .WillOnce(testing::Return(SendPacketStatus::kError));
  EXPECT_CALL(on_send_fn_, Call(_, SendPacketStatus::kError));
  EXPECT_FALSE(sender_.Send(PacketBuilder().Add(CookieAckChunk())));
}

TEST_F(PacketSenderTest, SendPacketWithTemporaryFailureQueuesPacket) {
  EXPECT_CALL(callbacks_, SendPacketWithStatus)
      .WillOnce(testing::Return(SendPacketStatus::kTemporaryFailure));

  EXPECT_FALSE(sender_.Send(PacketBuilder().Add(CookieAckChunk())));

  EXPECT_CALL(callbacks_, SendPacketWithStatus).Times(0);
  RunTimers();

  EXPECT_CALL(callbacks_, SendPacketWithStatus)
      .WillOnce(testing::Return(SendPacketStatus::kSuccess));
  callbacks_.AdvanceTime(DurationMs(1));
  RunTimers();

  // It's not enqueued any longer
  EXPECT_CALL(callbacks_, SendPacketWithStatus).Times(0);
  callbacks_.AdvanceTime(DurationMs(10));
  RunTimers();
}

}  // namespace
}  // namespace dcsctp
