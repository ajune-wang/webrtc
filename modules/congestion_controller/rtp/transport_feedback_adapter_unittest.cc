/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/transport_feedback_adapter.h"

#include <limits>
#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "modules/congestion_controller/rtp/congestion_controller_unittests_helper.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr PacedPacketInfo kPacingInfo0(0, 5, 2000);
constexpr PacedPacketInfo kPacingInfo1(1, 8, 4000);
constexpr PacedPacketInfo kPacingInfo2(2, 14, 7000);
constexpr PacedPacketInfo kPacingInfo3(3, 20, 10000);
constexpr PacedPacketInfo kPacingInfo4(4, 22, 10000);
constexpr uint32_t kSsrc = 8492;

class MockPacketFeedbackObserver : public PacketFeedbackObserver {
 public:
  MOCK_METHOD2(OnPacketAdded, void(uint32_t ssrc, uint16_t seq_num));
  MOCK_METHOD1(OnPacketFeedbackVector,
               void(const std::vector<PacketFeedback>& packet_feedback_vector));
};

void OnSentPacket(const PacketFeedback& packet_feedback,
                  Timestamp now,
                  TransportFeedbackAdapter* adapter) {
  RtpPacketSendInfo packet_info;
  packet_info.ssrc = kSsrc;
  packet_info.transport_sequence_number = packet_feedback.sequence_number;
  packet_info.rtp_sequence_number = 0;
  packet_info.has_rtp_sequence_number = true;
  packet_info.length = packet_feedback.payload_size;
  packet_info.pacing_info = packet_feedback.pacing_info;
  adapter->AddPacket(RtpPacketSendInfo(packet_info), 0u, now);
  adapter->ProcessSentPacket(rtc::SentPacket(packet_feedback.sequence_number,
                                             packet_feedback.send_time_ms,
                                             rtc::PacketInfo()));
}

TEST(TransportFeedbackAdapterTest, ObserverSanity) {
  TransportFeedbackAdapter adapter;
  Timestamp now = Timestamp::ms(0);

  MockPacketFeedbackObserver mock;
  adapter.RegisterPacketFeedbackObserver(&mock);

  const std::vector<PacketFeedback> packets = {
      PacketFeedback(100, 200, 0, 1000, kPacingInfo0),
      PacketFeedback(110, 210, 1, 2000, kPacingInfo0),
      PacketFeedback(120, 220, 2, 3000, kPacingInfo0)};

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sequence_number,
                   packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : packets) {
    EXPECT_CALL(mock, OnPacketAdded(kSsrc, packet.sequence_number)).Times(1);
    OnSentPacket(packet, now, &adapter);
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                           packet.arrival_time_ms * 1000));
  }

  EXPECT_CALL(mock, OnPacketFeedbackVector).Times(1);
  adapter.ProcessTransportFeedback(feedback, now);

  adapter.DeRegisterPacketFeedbackObserver(&mock);

  // After deregistration, the observer no longers gets indications.
  EXPECT_CALL(mock, OnPacketAdded).Times(0);
  const PacketFeedback new_packet(130, 230, 3, 4000, kPacingInfo0);
  OnSentPacket(new_packet, now, &adapter);

  rtcp::TransportFeedback second_feedback;
  second_feedback.SetBase(new_packet.sequence_number,
                          new_packet.arrival_time_ms * 1000);
  EXPECT_TRUE(feedback.AddReceivedPacket(new_packet.sequence_number,
                                         new_packet.arrival_time_ms * 1000));
  EXPECT_CALL(mock, OnPacketFeedbackVector).Times(0);
  adapter.ProcessTransportFeedback(second_feedback, now);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(TransportFeedbackAdapterTest, ObserverDoubleRegistrationDeathTest) {
  TransportFeedbackAdapter adapter;
  MockPacketFeedbackObserver mock;
  adapter.RegisterPacketFeedbackObserver(&mock);
  EXPECT_DEATH(adapter.RegisterPacketFeedbackObserver(&mock), "");
  adapter.DeRegisterPacketFeedbackObserver(&mock);
}

TEST(TransportFeedbackAdapterTest, ObserverMissingDeRegistrationDeathTest) {
  auto adapter = absl::make_unique<TransportFeedbackAdapter>();
  MockPacketFeedbackObserver mock;
  adapter->RegisterPacketFeedbackObserver(&mock);
  EXPECT_DEATH(adapter.reset(), "");
  adapter->DeRegisterPacketFeedbackObserver(&mock);
}
#endif

TEST(TransportFeedbackAdapterTest, AdaptsFeedbackAndPopulatesSendTimes) {
  TransportFeedbackAdapter adapter;
  Timestamp now = Timestamp::ms(0);

  std::vector<PacketFeedback> packets = {
      PacketFeedback(100, 200, 0, 1500, kPacingInfo0),
      PacketFeedback(110, 210, 1, 1500, kPacingInfo0),
      PacketFeedback(120, 220, 2, 1500, kPacingInfo0),
      PacketFeedback(130, 230, 3, 1500, kPacingInfo1),
      PacketFeedback(140, 240, 4, 1500, kPacingInfo1)};

  for (const PacketFeedback& packet : packets)
    OnSentPacket(packet, now, &adapter);

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sequence_number,
                   packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : packets) {
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                           packet.arrival_time_ms * 1000));
  }

  feedback.Build();

  adapter.ProcessTransportFeedback(feedback, now);
  ComparePacketFeedbackVectors(packets, adapter.GetTransportFeedbackVector());
}

TEST(TransportFeedbackAdapterTest, FeedbackVectorReportsUnreceived) {
  TransportFeedbackAdapter adapter;
  Timestamp now = Timestamp::ms(0);

  std::vector<PacketFeedback> sent_packets = {
      PacketFeedback(100, 220, 0, 1500, kPacingInfo0),
      PacketFeedback(110, 210, 1, 1500, kPacingInfo0),
      PacketFeedback(120, 220, 2, 1500, kPacingInfo0),
      PacketFeedback(130, 230, 3, 1500, kPacingInfo0),
      PacketFeedback(140, 240, 4, 1500, kPacingInfo0),
      PacketFeedback(150, 250, 5, 1500, kPacingInfo0),
      PacketFeedback(160, 260, 6, 1500, kPacingInfo0)};

  for (const PacketFeedback& packet : sent_packets)
    OnSentPacket(packet, now, &adapter);

  // Note: Important to include the last packet, as only unreceived packets in
  // between received packets can be inferred.
  std::vector<PacketFeedback> received_packets = {
      sent_packets[0], sent_packets[2], sent_packets[6]};

  rtcp::TransportFeedback feedback;
  feedback.SetBase(received_packets[0].sequence_number,
                   received_packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : received_packets) {
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                           packet.arrival_time_ms * 1000));
  }

  feedback.Build();

  adapter.ProcessTransportFeedback(feedback, now);
  ComparePacketFeedbackVectors(sent_packets,
                               adapter.GetTransportFeedbackVector());
}

TEST(TransportFeedbackAdapterTest, HandlesDroppedPackets) {
  TransportFeedbackAdapter adapter;
  Timestamp now = Timestamp::ms(0);
  std::vector<PacketFeedback> packets = {
      PacketFeedback(100, 200, 0, 1500, kPacingInfo0),
      PacketFeedback(110, 210, 1, 1500, kPacingInfo1),
      PacketFeedback(120, 220, 2, 1500, kPacingInfo2),
      PacketFeedback(130, 230, 3, 1500, kPacingInfo3),
      PacketFeedback(140, 240, 4, 1500, kPacingInfo4)};

  const uint16_t kSendSideDropBefore = 1;
  const uint16_t kReceiveSideDropAfter = 3;

  for (const PacketFeedback& packet : packets) {
    if (packet.sequence_number >= kSendSideDropBefore)
      OnSentPacket(packet, now, &adapter);
  }

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sequence_number,
                   packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : packets) {
    if (packet.sequence_number <= kReceiveSideDropAfter) {
      EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                             packet.arrival_time_ms * 1000));
    }
  }

  feedback.Build();

  std::vector<PacketFeedback> expected_packets(
      packets.begin(), packets.begin() + kReceiveSideDropAfter + 1);
  // Packets that have timed out on the send-side have lost the
  // information stored on the send-side.
  for (size_t i = 0; i < kSendSideDropBefore; ++i) {
    expected_packets[i].send_time_ms = -1;
    expected_packets[i].payload_size = 0;
    expected_packets[i].pacing_info = PacedPacketInfo();
  }

  adapter.ProcessTransportFeedback(feedback, now);
  ComparePacketFeedbackVectors(expected_packets,
                               adapter.GetTransportFeedbackVector());
}

TEST(TransportFeedbackAdapterTest, SendTimeWrapsBothWays) {
  TransportFeedbackAdapter adapter;
  Timestamp now = Timestamp::ms(0);

  int64_t kHighArrivalTimeMs = rtcp::TransportFeedback::kDeltaScaleFactor *
                               static_cast<int64_t>(1 << 8) *
                               static_cast<int64_t>((1 << 23) - 1) / 1000;
  std::vector<PacketFeedback> packets = {
      PacketFeedback(kHighArrivalTimeMs - 64, 200, 0, 1500, PacedPacketInfo()),
      PacketFeedback(kHighArrivalTimeMs + 64, 210, 1, 1500, PacedPacketInfo()),
      PacketFeedback(kHighArrivalTimeMs, 220, 2, 1500, PacedPacketInfo())};

  for (const PacketFeedback& packet : packets)
    OnSentPacket(packet, now, &adapter);

  for (size_t i = 0; i < packets.size(); ++i) {
    auto feedback = absl::make_unique<rtcp::TransportFeedback>();
    feedback->SetBase(packets[i].sequence_number,
                      packets[i].arrival_time_ms * 1000);

    EXPECT_TRUE(feedback->AddReceivedPacket(packets[i].sequence_number,
                                            packets[i].arrival_time_ms * 1000));

    rtc::Buffer raw_packet = feedback->Build();
    feedback = rtcp::TransportFeedback::ParseFrom(raw_packet.data(),
                                                  raw_packet.size());

    std::vector<PacketFeedback> expected_packets = {packets[i]};

    adapter.ProcessTransportFeedback(*feedback, now);
    ComparePacketFeedbackVectors(expected_packets,
                                 adapter.GetTransportFeedbackVector());
  }
}

TEST(TransportFeedbackAdapterTest, HandlesArrivalReordering) {
  TransportFeedbackAdapter adapter;
  Timestamp now = Timestamp::ms(0);

  std::vector<PacketFeedback> packets = {
      PacketFeedback(120, 200, 0, 1500, kPacingInfo0),
      PacketFeedback(110, 210, 1, 1500, kPacingInfo0),
      PacketFeedback(100, 220, 2, 1500, kPacingInfo0)};

  for (const PacketFeedback& packet : packets)
    OnSentPacket(packet, now, &adapter);

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sequence_number,
                   packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : packets) {
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                           packet.arrival_time_ms * 1000));
  }

  feedback.Build();

  // Adapter keeps the packets ordered by sequence number (which is itself
  // assigned by the order of transmission). Reordering by some other criteria,
  // eg. arrival time, is up to the observers.
  adapter.ProcessTransportFeedback(feedback, now);
  ComparePacketFeedbackVectors(packets, adapter.GetTransportFeedbackVector());
}

TEST(TransportFeedbackAdapterTest, TimestampDeltas) {
  TransportFeedbackAdapter adapter;
  Timestamp now = Timestamp::ms(0);

  std::vector<PacketFeedback> sent_packets;
  const int64_t kSmallDeltaUs =
      rtcp::TransportFeedback::kDeltaScaleFactor * ((1 << 8) - 1);
  const int64_t kLargePositiveDeltaUs =
      rtcp::TransportFeedback::kDeltaScaleFactor *
      std::numeric_limits<int16_t>::max();
  const int64_t kLargeNegativeDeltaUs =
      rtcp::TransportFeedback::kDeltaScaleFactor *
      std::numeric_limits<int16_t>::min();

  PacketFeedback packet_feedback(100, 200, 0, 1500, true, 0, 0,
                                 PacedPacketInfo());
  sent_packets.push_back(packet_feedback);

  packet_feedback.send_time_ms += kSmallDeltaUs / 1000;
  packet_feedback.arrival_time_ms += kSmallDeltaUs / 1000;
  ++packet_feedback.sequence_number;
  sent_packets.push_back(packet_feedback);

  packet_feedback.send_time_ms += kLargePositiveDeltaUs / 1000;
  packet_feedback.arrival_time_ms += kLargePositiveDeltaUs / 1000;
  ++packet_feedback.sequence_number;
  sent_packets.push_back(packet_feedback);

  packet_feedback.send_time_ms += kLargeNegativeDeltaUs / 1000;
  packet_feedback.arrival_time_ms += kLargeNegativeDeltaUs / 1000;
  ++packet_feedback.sequence_number;
  sent_packets.push_back(packet_feedback);

  // Too large, delta - will need two feedback messages.
  packet_feedback.send_time_ms += (kLargePositiveDeltaUs + 1000) / 1000;
  packet_feedback.arrival_time_ms += (kLargePositiveDeltaUs + 1000) / 1000;
  ++packet_feedback.sequence_number;

  // Packets will be added to send history.
  for (const PacketFeedback& packet : sent_packets)
    OnSentPacket(packet, now, &adapter);
  OnSentPacket(packet_feedback, now, &adapter);

  // Create expected feedback and send into adapter.
  auto feedback = absl::make_unique<rtcp::TransportFeedback>();
  feedback->SetBase(sent_packets[0].sequence_number,
                    sent_packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : sent_packets) {
    EXPECT_TRUE(feedback->AddReceivedPacket(packet.sequence_number,
                                            packet.arrival_time_ms * 1000));
  }
  EXPECT_FALSE(feedback->AddReceivedPacket(
      packet_feedback.sequence_number, packet_feedback.arrival_time_ms * 1000));

  rtc::Buffer raw_packet = feedback->Build();
  feedback =
      rtcp::TransportFeedback::ParseFrom(raw_packet.data(), raw_packet.size());

  std::vector<PacketFeedback> received_feedback;

  EXPECT_TRUE(feedback != nullptr);
  adapter.ProcessTransportFeedback(*feedback, now);
  ComparePacketFeedbackVectors(sent_packets,
                               adapter.GetTransportFeedbackVector());

  // Create a new feedback message and add the trailing item.
  feedback = absl::make_unique<rtcp::TransportFeedback>();
  feedback->SetBase(packet_feedback.sequence_number,
                    packet_feedback.arrival_time_ms * 1000);
  EXPECT_TRUE(feedback->AddReceivedPacket(
      packet_feedback.sequence_number, packet_feedback.arrival_time_ms * 1000));
  raw_packet = feedback->Build();
  feedback =
      rtcp::TransportFeedback::ParseFrom(raw_packet.data(), raw_packet.size());

  EXPECT_TRUE(feedback != nullptr);
  adapter.ProcessTransportFeedback(*feedback, now);
  {
    std::vector<PacketFeedback> expected_packets = {packet_feedback};
    ComparePacketFeedbackVectors(expected_packets,
                                 adapter.GetTransportFeedbackVector());
  }
}

TEST(TransportFeedbackAdapterTest, IgnoreDuplicatePacketSentCalls) {
  TransportFeedbackAdapter adapter;
  Timestamp now = Timestamp::ms(0);

  const PacketFeedback packet(100, 200, 0, 1500, kPacingInfo0);

  // Add a packet and then mark it as sent.
  RtpPacketSendInfo packet_info;
  packet_info.ssrc = kSsrc;
  packet_info.transport_sequence_number = packet.sequence_number;
  packet_info.length = packet.payload_size;
  packet_info.pacing_info = packet.pacing_info;
  adapter.AddPacket(packet_info, 0u, now);
  absl::optional<SentPacket> sent_packet =
      adapter.ProcessSentPacket(rtc::SentPacket(
          packet.sequence_number, packet.send_time_ms, rtc::PacketInfo()));
  EXPECT_TRUE(sent_packet.has_value());

  // Call ProcessSentPacket() again with the same sequence number. This packet
  // has already been marked as sent and the call should be ignored.
  absl::optional<SentPacket> duplicate_packet =
      adapter.ProcessSentPacket(rtc::SentPacket(
          packet.sequence_number, packet.send_time_ms, rtc::PacketInfo()));
  EXPECT_FALSE(duplicate_packet.has_value());
}

TEST(TransportFeedbackAdapterTest, AllowDuplicatePacketSentCallsWithTrial) {
  // Allow duplicates if this field trial kill-switch is enabled.
  webrtc::test::ScopedFieldTrials field_trial(
      "WebRTC-TransportFeedbackAdapter-AllowDuplicates/Enabled/");
  TransportFeedbackAdapter adapter;
  Timestamp now = Timestamp::ms(0);

  const PacketFeedback packet(100, 200, 0, 1500, kPacingInfo0);

  // Add a packet and then mark it as sent.
  RtpPacketSendInfo packet_info;
  packet_info.ssrc = kSsrc;
  packet_info.transport_sequence_number = packet.sequence_number;
  packet_info.length = packet.payload_size;
  packet_info.pacing_info = packet.pacing_info;
  adapter.AddPacket(packet_info, 0u, now);
  absl::optional<SentPacket> sent_packet =
      adapter.ProcessSentPacket(rtc::SentPacket(
          packet.sequence_number, packet.send_time_ms, rtc::PacketInfo()));
  EXPECT_TRUE(sent_packet.has_value());

  // Call ProcessSentPacket() again with the same sequence number. This packet
  // should still be allowed due to the field trial/
  absl::optional<SentPacket> duplicate_packet =
      adapter.ProcessSentPacket(rtc::SentPacket(
          packet.sequence_number, packet.send_time_ms, rtc::PacketInfo()));
  EXPECT_TRUE(duplicate_packet.has_value());
}

}  // namespace
}  // namespace webrtc
