/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_packet_history.h"

#include <memory>
#include <utility>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

typedef RtpPacketHistory::StorageMode StorageMode;

class RtpPacketHistoryTest : public ::testing::Test {
 protected:
  static constexpr uint16_t kStartSeqNum = 88;

  RtpPacketHistoryTest() : fake_clock_(123456), hist_(&fake_clock_) {}

  SimulatedClock fake_clock_;
  RtpPacketHistory hist_;

  std::unique_ptr<RtpPacketToSend> CreateRtpPacket(uint16_t seq_num) {
    // Payload, ssrc, timestamp and extensions are irrelevant for this tests.
    std::unique_ptr<RtpPacketToSend> packet(new RtpPacketToSend(nullptr));
    packet->SetSequenceNumber(seq_num);
    packet->set_capture_time_ms(fake_clock_.TimeInMilliseconds());
    return packet;
  }
};

const uint16_t RtpPacketHistoryTest::kStartSeqNum;

TEST_F(RtpPacketHistoryTest, SetStoreStatus) {
  EXPECT_EQ(StorageMode::kDisabled, hist_.GetStorageMode());
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  EXPECT_EQ(StorageMode::kStore, hist_.GetStorageMode());
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  EXPECT_EQ(StorageMode::kStoreAndCull, hist_.GetStorageMode());
  hist_.SetStorePacketsStatus(StorageMode::kDisabled, 0);
  EXPECT_EQ(StorageMode::kDisabled, hist_.GetStorageMode());
}

TEST_F(RtpPacketHistoryTest, ClearsHistoryAfterSetStoreStatus) {
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  // Store a packet, but with send-time. It should then not be removed.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), kAllowRetransmission,
                     rtc::nullopt);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Changing store status, even to the current one, will clear the history.
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, NoStoreStatus) {
  EXPECT_EQ(StorageMode::kDisabled, hist_.GetStorageMode());
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, rtc::nullopt);
  // Packet should not be stored.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, GetRtpPacket_NotStored) {
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  EXPECT_FALSE(hist_.GetPacketState(0, false));
}

TEST_F(RtpPacketHistoryTest, PutRtpPacket) {
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);

  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, rtc::nullopt);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, GetRtpPacket) {
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  int64_t capture_time_ms = 1;
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->set_capture_time_ms(capture_time_ms);
  rtc::CopyOnWriteBuffer buffer = packet->Buffer();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, rtc::nullopt);

  std::unique_ptr<RtpPacketToSend> packet_out =
      hist_.GetPacketAndSetSendTime(kStartSeqNum, false);
  EXPECT_TRUE(packet_out);
  EXPECT_EQ(buffer, packet_out->Buffer());
  EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());
}

TEST_F(RtpPacketHistoryTest, NoCaptureTime) {
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  fake_clock_.AdvanceTimeMilliseconds(1);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->set_capture_time_ms(-1);
  rtc::CopyOnWriteBuffer buffer = packet->Buffer();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, rtc::nullopt);

  std::unique_ptr<RtpPacketToSend> packet_out =
      hist_.GetPacketAndSetSendTime(kStartSeqNum, false);
  EXPECT_TRUE(packet_out);
  EXPECT_EQ(buffer, packet_out->Buffer());
  EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());
}

TEST_F(RtpPacketHistoryTest, DontRetransmit) {
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  rtc::CopyOnWriteBuffer buffer = packet->Buffer();
  hist_.PutRtpPacket(std::move(packet), kDontRetransmit, rtc::nullopt);

  // Get the packet and verify data.
  std::unique_ptr<RtpPacketToSend> packet_out;
  packet_out = hist_.GetPacketAndSetSendTime(kStartSeqNum, false);
  ASSERT_TRUE(packet_out);
  EXPECT_EQ(buffer.size(), packet_out->size());
  EXPECT_EQ(capture_time_ms, packet_out->capture_time_ms());

  // Non-retransmittable packets are immediately removed, so getting in again
  // should fail.
  EXPECT_FALSE(hist_.GetPacketAndSetSendTime(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, PacketStateIsCorrect) {
  const uint32_t kSsrc = 92384762;
  const uint16_t kTransportStartSeqNo = 12345;
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->SetSsrc(kSsrc);
  packet->SetPayloadSize(1234);
  const size_t packet_size = packet->size();

  hist_.PutRtpPacket(std::move(packet), StorageType::kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  hist_.OnTransportSequenceCreated(kStartSeqNum, kTransportStartSeqNo);

  rtc::Optional<RtpPacketHistory::PacketState> state =
      hist_.GetPacketState(kStartSeqNum, false);
  ASSERT_TRUE(state);
  EXPECT_EQ(state->rtp_sequence_number, kStartSeqNum);
  EXPECT_EQ(state->transport_sequence_number, kTransportStartSeqNo);
  EXPECT_EQ(state->send_time_ms, fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(state->capture_time_ms, fake_clock_.TimeInMilliseconds());
  EXPECT_EQ(state->ssrc, kSsrc);
  EXPECT_EQ(state->payload_size, packet_size);
  EXPECT_EQ(state->times_retransmitted, 0u);

  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum, false));

  state = hist_.GetPacketState(kStartSeqNum, false);
  ASSERT_TRUE(state);
  EXPECT_EQ(state->times_retransmitted, 1u);
}

TEST_F(RtpPacketHistoryTest, MinResendTimeWithPacer) {
  static const int64_t kMinRetransmitIntervalMs = 100;

  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  hist_.SetRtt(kMinRetransmitIntervalMs);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  size_t len = packet->size();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, rtc::nullopt);

  // First transmission: TimeToSendPacket() call from pacer.
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum, false));

  // First retransmission - allow early retransmission.
  fake_clock_.AdvanceTimeMilliseconds(1);
  rtc::Optional<RtpPacketHistory::PacketState> packet_state =
      hist_.GetPacketState(kStartSeqNum, true);
  EXPECT_TRUE(packet_state);
  EXPECT_EQ(len, packet_state->payload_size);
  EXPECT_EQ(capture_time_ms, packet_state->capture_time_ms);

  // Retransmission was allowed, next send it from pacer.
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum, false));

  // Second retransmission - advance time to just before retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs - 1);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, true));

  // Advance time to just after retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, true));
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, MinResendTimeWithoutPacer) {
  static const int64_t kMinRetransmitIntervalMs = 100;

  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);
  hist_.SetRtt(kMinRetransmitIntervalMs);
  int64_t capture_time_ms = fake_clock_.TimeInMilliseconds();
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  size_t len = packet->size();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  // First retransmission - allow early retransmission.
  fake_clock_.AdvanceTimeMilliseconds(1);
  packet = hist_.GetPacketAndSetSendTime(kStartSeqNum, true);
  EXPECT_TRUE(packet);
  EXPECT_EQ(len, packet->size());
  EXPECT_EQ(capture_time_ms, packet->capture_time_ms());

  // Second retransmission - advance time to just before retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(kMinRetransmitIntervalMs - 1);
  EXPECT_FALSE(hist_.GetPacketAndSetSendTime(kStartSeqNum, true));

  // Advance time to just after retransmission OK.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum, true));
}

TEST_F(RtpPacketHistoryTest, RemovesPacketsWhenFull) {
  const size_t kMaxNumPackets = 10;
  hist_.SetStorePacketsStatus(StorageMode::kStore, kMaxNumPackets);

  // History does not allow removing packets within kMinPacketDurationMs,
  // so in order to test capacity, make sure insertion spans this time.
  const int64_t kPacketIntervalMs =
      RtpPacketHistory::kMinPacketDurationMs / kMaxNumPackets;

  // Add packets until the buffer is full.
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum + i);
    // Immediate mark packet as sent.
    hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                       fake_clock_.TimeInMilliseconds());
    fake_clock_.AdvanceTimeMilliseconds(kPacketIntervalMs);
  }

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // History is full, oldest one should be overwritten.
  std::unique_ptr<RtpPacketToSend> packet =
      CreateRtpPacket(kStartSeqNum + kMaxNumPackets);
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  // Oldest packet should be gone, but packet after than one still present.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum + 1, false));
}

TEST_F(RtpPacketHistoryTest, RemovesPacketsWhenReallyFull) {
  // Tests the absolute upper bound on number of stored packets. Don't allow
  // storing more than this, even if packets have not yet been sent.
  const size_t kMaxNumPackets = RtpPacketHistory::kMaxCapacity;
  hist_.SetStorePacketsStatus(StorageMode::kStore,
                              RtpPacketHistory::kMaxCapacity + 1);

  // Add packets until the buffer is full.
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum + i);
    // Don't mark packets as sent, preventing them from being removed.
    hist_.PutRtpPacket(std::move(packet), kAllowRetransmission, rtc::nullopt);
  }

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // History is full, oldest one should be overwritten.
  std::unique_ptr<RtpPacketToSend> packet =
      CreateRtpPacket(kStartSeqNum + kMaxNumPackets);
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  // Oldest packet should be gone, but packet after than one still present.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum + 1, false));
}

TEST_F(RtpPacketHistoryTest, DontRemoveUnsentPackets) {
  const size_t kMaxNumPackets = 10;
  hist_.SetStorePacketsStatus(StorageMode::kStore, kMaxNumPackets);

  // Add packets until the buffer is full.
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    // Mark packets as unsent.
    hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + i), kAllowRetransmission,
                       rtc::nullopt);
  }
  fake_clock_.AdvanceTimeMilliseconds(RtpPacketHistory::kMinPacketDurationMs);

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // History is full, but old packets not sent, so allow expansion.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + kMaxNumPackets),
                     kAllowRetransmission, fake_clock_.TimeInMilliseconds());
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Set all packet as sent and advance time past min packet duration time,
  // otherwise packets till still be prevented from being removed.
  for (size_t i = 0; i <= kMaxNumPackets; ++i) {
    EXPECT_TRUE(hist_.GetPacketAndSetSendTime(kStartSeqNum + i, false));
  }
  fake_clock_.AdvanceTimeMilliseconds(RtpPacketHistory::kMinPacketDurationMs);
  // Add a new packet, this means the two oldest ones will be culled.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + kMaxNumPackets + 1),
                     kAllowRetransmission, fake_clock_.TimeInMilliseconds());
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum + 1, false));
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum + 2, false));
}

TEST_F(RtpPacketHistoryTest, DontRemoveTooRecentlyTransmittedPackets) {
  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStore, 1);

  // Add a packet, marked as send, and advance time to just before removal time.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  fake_clock_.AdvanceTimeMilliseconds(RtpPacketHistory::kMinPacketDurationMs -
                                      1);

  // Add a new packet to trigger culling.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Advance time to where packet will be eligible for removal and try again.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 2), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  // First packet should no be gone, but next one still there.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum + 1, false));
}

TEST_F(RtpPacketHistoryTest, DontRemoveTooRecentlyTransmittedPacketsHighRtt) {
  const int64_t kRttMs = RtpPacketHistory::kMinPacketDurationMs * 2;
  const int64_t kPacketTimeoutMs =
      kRttMs * RtpPacketHistory::kMinPacketDurationRtt;

  // Set size to remove old packets as soon as possible.
  hist_.SetStorePacketsStatus(StorageMode::kStore, 1);
  hist_.SetRtt(kRttMs);

  // Add a packet, marked as send, and advance time to just before removal time.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeoutMs - 1);

  // Add a new packet to trigger culling.
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Advance time to where packet will be eligible for removal and try again.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 2), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  // First packet should no be gone, but next one still there.
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum + 1, false));
}

TEST_F(RtpPacketHistoryTest, RemovesOldWithCulling) {
  const size_t kMaxNumPackets = 10;
  // Enable culling. Even without feedback, this can trigger early removal.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kMaxNumPackets);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  int64_t kMaxPacketDurationMs = RtpPacketHistory::kMinPacketDurationMs *
                                 RtpPacketHistory::kPacketCullingDelayFactor;
  fake_clock_.AdvanceTimeMilliseconds(kMaxPacketDurationMs - 1);

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Advance to where packet can be culled, even if buffer is not full.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, RemovesOldWithCullingHighRtt) {
  const size_t kMaxNumPackets = 10;
  const int64_t kRttMs = RtpPacketHistory::kMinPacketDurationMs * 2;
  // Enable culling. Even without feedback, this can trigger early removal.
  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, kMaxNumPackets);
  hist_.SetRtt(kRttMs);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  int64_t kMaxPacketDurationMs = kRttMs *
                                 RtpPacketHistory::kMinPacketDurationRtt *
                                 RtpPacketHistory::kPacketCullingDelayFactor;
  fake_clock_.AdvanceTimeMilliseconds(kMaxPacketDurationMs - 1);

  // First packet should still be there.
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));

  // Advance to where packet can be culled, even if buffer is not full.
  fake_clock_.AdvanceTimeMilliseconds(1);
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
}

TEST_F(RtpPacketHistoryTest, CullsPacketsWithTransportFeedback) {
  const uint16_t kTransportStartSeqNum = 65534;

  hist_.SetStorePacketsStatus(StorageMode::kStoreAndCull, 10);

  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 1), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  hist_.PutRtpPacket(CreateRtpPacket(kStartSeqNum + 2), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  // Adding transport sequence number for non-existent pack is a noop.
  hist_.OnTransportSequenceCreated(kStartSeqNum - 1, kTransportStartSeqNum - 1);

  // Add transport seq for all three packets.
  hist_.OnTransportSequenceCreated(kStartSeqNum, kTransportStartSeqNum);
  hist_.OnTransportSequenceCreated(kStartSeqNum + 1, kTransportStartSeqNum + 1);
  hist_.OnTransportSequenceCreated(
      kStartSeqNum + 2, static_cast<uint16_t>(kTransportStartSeqNum + 2));

  // Report feedback only for the middle one.
  std::vector<PacketFeedback> feedback;
  feedback.emplace_back(fake_clock_.TimeInMilliseconds(),
                        kTransportStartSeqNum + 1);
  hist_.OnTransportFeedback(feedback);

  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum + 1, false));
  EXPECT_TRUE(hist_.GetPacketState(kStartSeqNum + 2, false));

  // Add feedback for the remaining two.
  feedback.clear();
  feedback.emplace_back(fake_clock_.TimeInMilliseconds(),
                        kTransportStartSeqNum);
  feedback.emplace_back(fake_clock_.TimeInMilliseconds(),
                        kTransportStartSeqNum + 2);
  hist_.OnTransportFeedback(feedback);
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum, false));
  EXPECT_FALSE(hist_.GetPacketState(kStartSeqNum + 2, false));
}

TEST_F(RtpPacketHistoryTest, GetBestFittingPacket) {
  const size_t kTargetSize = 500;
  hist_.SetStorePacketsStatus(StorageMode::kStore, 10);

  // Add three packets of various sizes.
  std::unique_ptr<RtpPacketToSend> packet = CreateRtpPacket(kStartSeqNum);
  packet->SetPayloadSize(kTargetSize);
  const size_t target_packet_size = packet->size();
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  packet = CreateRtpPacket(kStartSeqNum + 1);
  packet->SetPayloadSize(kTargetSize - 1);
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());
  packet = CreateRtpPacket(kStartSeqNum + 2);
  packet->SetPayloadSize(kTargetSize + 1);
  hist_.PutRtpPacket(std::move(packet), kAllowRetransmission,
                     fake_clock_.TimeInMilliseconds());

  EXPECT_EQ(target_packet_size,
            hist_.GetBestFittingPacket(target_packet_size)->size());
}
}  // namespace webrtc
