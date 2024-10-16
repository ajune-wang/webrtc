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

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "api/transport/network_types.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/ntp_time_util.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/network/ecn_marking.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace webrtc {
namespace {
constexpr uint32_t kSsrc = 8492;
const PacedPacketInfo kPacingInfo0(0, 5, 2000);
const PacedPacketInfo kPacingInfo1(1, 8, 4000);
const PacedPacketInfo kPacingInfo2(2, 14, 7000);
const PacedPacketInfo kPacingInfo3(3, 20, 10000);
const PacedPacketInfo kPacingInfo4(4, 22, 10000);

void ComparePacketFeedbackVectors(const std::vector<PacketResult>& truth,
                                  const std::vector<PacketResult>& input) {
  ASSERT_EQ(truth.size(), input.size());
  size_t len = truth.size();
  // truth contains the input data for the test, and input is what will be
  // sent to the bandwidth estimator. truth.arrival_tims_ms is used to
  // populate the transport feedback messages. As these times may be changed
  // (because of resolution limits in the packets, and because of the time
  // base adjustment performed by the TransportFeedbackAdapter at the first
  // packet, the truth[x].arrival_time and input[x].arrival_time may not be
  // equal. However, the difference must be the same for all x.
  TimeDelta arrival_time_delta = truth[0].receive_time - input[0].receive_time;
  for (size_t i = 0; i < len; ++i) {
    RTC_CHECK(truth[i].IsReceived());
    if (input[i].IsReceived()) {
      EXPECT_EQ(truth[i].receive_time - input[i].receive_time,
                arrival_time_delta);
    }
    EXPECT_EQ(truth[i].sent_packet.send_time, input[i].sent_packet.send_time);
    EXPECT_EQ(truth[i].sent_packet.sequence_number,
              input[i].sent_packet.sequence_number);
    EXPECT_EQ(truth[i].sent_packet.size, input[i].sent_packet.size);
    EXPECT_EQ(truth[i].sent_packet.pacing_info,
              input[i].sent_packet.pacing_info);
  }
}

struct PacketTemplate {
  uint32_t ssrc = 1;
  int64_t transport_sequence_number = 0;
  uint16_t rtp_sequence_number = 2;
  RtpPacketMediaType media_type = RtpPacketMediaType::kVideo;
  DataSize packet_size = DataSize::Bytes(100);

  rtc::EcnMarking ecn = rtc::EcnMarking::kNotEct;
  Timestamp send_timestamp = Timestamp::Millis(0);
  PacedPacketInfo pacing_info;
  Timestamp receive_timestamp = Timestamp::MinusInfinity();
};

void ComparePacketFeedbackVectors(const std::vector<PacketTemplate>& truth,
                                  const std::vector<PacketResult>& input) {
  ASSERT_EQ(truth.size(), input.size());
  size_t len = truth.size();
  // truth contains the input data for the test, and input is what will be
  // sent to the bandwidth estimator. truth.arrival_tims_ms is used to
  // populate the transport feedback messages. As these times may be changed
  // (because of resolution limits in the packets, and because of the time
  // base adjustment performed by the TransportFeedbackAdapter at the first
  // packet, the truth[x].arrival_time and input[x].arrival_time may not be
  // equal. However, the difference must be the same for all x.
  TimeDelta arrival_time_delta =
      truth[0].receive_timestamp - input[0].receive_time;
  for (size_t i = 0; i < len; ++i) {
    EXPECT_EQ(truth[i].receive_timestamp.IsFinite(), input[i].IsReceived());
    if (input[i].IsReceived()) {
      EXPECT_EQ(truth[i].receive_timestamp - input[i].receive_time,
                arrival_time_delta);
    }
    EXPECT_EQ(truth[i].send_timestamp, input[i].sent_packet.send_time);
    EXPECT_EQ(truth[i].transport_sequence_number,
              input[i].sent_packet.sequence_number);
    EXPECT_EQ(truth[i].packet_size, input[i].sent_packet.size);
    EXPECT_EQ(truth[i].pacing_info, input[i].sent_packet.pacing_info);
  }
}

PacketResult CreatePacket(int64_t receive_time_ms,
                          int64_t send_time_ms,
                          int64_t sequence_number,
                          size_t payload_size,
                          const PacedPacketInfo& pacing_info) {
  PacketResult res;
  res.receive_time = Timestamp::Millis(receive_time_ms);
  res.sent_packet.send_time = Timestamp::Millis(send_time_ms);
  res.sent_packet.sequence_number = sequence_number;
  res.sent_packet.size = DataSize::Bytes(payload_size);
  res.sent_packet.pacing_info = pacing_info;
  return res;
}

RtpPacketToSend CreatePacketToSend(PacketTemplate packet,
                                   bool set_header_extensions) {
  RtpHeaderExtensionMap extensions;
  if (set_header_extensions) {
    extensions.Register<TransportSequenceNumber>(1);
  }
  RtpPacketToSend send_packet(&extensions);
  if (set_header_extensions) {
    send_packet.SetExtension<TransportSequenceNumber>(
        packet.transport_sequence_number & 0xFFFF);
  }
  send_packet.SetSsrc(packet.ssrc);
  send_packet.SetPayloadSize(packet.packet_size.bytes() -
                             send_packet.headers_size());
  send_packet.SetSequenceNumber(packet.rtp_sequence_number);
  send_packet.set_transport_sequence_number(packet.transport_sequence_number);

  return send_packet;
}

rtcp::TransportFeedback BuildRtcpTransportFeedbackPacket(
    const std::vector<PacketTemplate>& packets) {
  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].transport_sequence_number,
                   packets[0].receive_timestamp);

  for (const auto& packet : packets) {
    if (packet.receive_timestamp.IsFinite()) {
      EXPECT_TRUE(feedback.AddReceivedPacket(packet.transport_sequence_number,
                                             packet.receive_timestamp));
    }
  }
  return feedback;
}

class MockStreamFeedbackObserver : public webrtc::StreamFeedbackObserver {
 public:
  MOCK_METHOD(void,
              OnPacketFeedbackVector,
              (std::vector<StreamPacketInfo> packet_feedback_vector),
              (override));
};

}  // namespace

class TransportFeedbackAdapterTest : public ::testing::TestWithParam<bool> {
 public:
  TransportFeedbackAdapterTest() : clock_(12310000) {}

  bool UseTransportLayerFeedback() const { return GetParam(); }

  virtual ~TransportFeedbackAdapterTest() {}

  virtual void SetUp() { adapter_.reset(new TransportFeedbackAdapter()); }

  virtual void TearDown() { adapter_.reset(); }

 protected:
  void OnSentPacket(const PacketResult& packet_feedback) {
    RtpPacketSendInfo packet_info;
    packet_info.media_ssrc = kSsrc;
    packet_info.transport_sequence_number =
        packet_feedback.sent_packet.sequence_number;
    packet_info.rtp_sequence_number = 0;
    packet_info.length = packet_feedback.sent_packet.size.bytes();
    packet_info.pacing_info = packet_feedback.sent_packet.pacing_info;
    packet_info.packet_type = RtpPacketMediaType::kVideo;
    adapter_->AddPacket(RtpPacketSendInfo(packet_info), 0u,
                        clock_.CurrentTime());
    adapter_->ProcessSentPacket(rtc::SentPacket(
        packet_feedback.sent_packet.sequence_number,
        packet_feedback.sent_packet.send_time.ms(), rtc::PacketInfo()));
  }

  rtcp::CongestionControlFeedback BuildRtcpCongestionControlFeedbackPacket(
      const std::vector<PacketTemplate>& packets) {
    // Assume the feedback was sent when the last packet was received.
    Timestamp feedback_sent_time = Timestamp::MinusInfinity();
    for (auto it = packets.crbegin(); it != packets.crend(); ++it) {
      if (it->receive_timestamp.IsFinite()) {
        feedback_sent_time = it->receive_timestamp;
        break;
      }
    }

    std::vector<rtcp::CongestionControlFeedback::PacketInfo> packet_infos;
    for (const auto& packet : packets) {
      if (packet.receive_timestamp.IsFinite()) {
        packet_infos.push_back(
            {.ssrc = packet.ssrc,
             .sequence_number = packet.rtp_sequence_number,
             .arrival_time_offset =
                 feedback_sent_time - packet.receive_timestamp});
      }
    }

    uint32_t compact_ntp =
        CompactNtp(clock_.ConvertTimestampToNtpTime(feedback_sent_time));
    return rtcp::CongestionControlFeedback(std::move(packet_infos),
                                           compact_ntp);
  }

  SimulatedClock clock_;
  std::unique_ptr<TransportFeedbackAdapter> adapter_;
};

INSTANTIATE_TEST_SUITE_P(FeedbackFormats,
                         TransportFeedbackAdapterTest,
                         ::testing::Bool(),
                         [](testing::TestParamInfo<bool> param) {
                           if (param.param)
                             return "CongestionControlFeedback";
                           else
                             return "TransportFeedback";
                         });

TEST_P(TransportFeedbackAdapterTest, AdaptsFeedbackAndPopulatesSendTimes) {
  TransportFeedbackAdapter adapter;

  std::vector<PacketTemplate> packets = {
      {
          .transport_sequence_number = 1,
          .rtp_sequence_number = 101,
          .send_timestamp = Timestamp::Millis(100),
          .pacing_info = kPacingInfo0,
          .receive_timestamp = Timestamp::Millis(200),
      },
      {
          .transport_sequence_number = 2,
          .rtp_sequence_number = 102,
          .send_timestamp = Timestamp::Millis(110),
          .pacing_info = kPacingInfo1,
          .receive_timestamp = Timestamp::Millis(210),
      }};

  for (const auto& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet, !UseTransportLayerFeedback()),
                      packet.pacing_info,
                      /*overhead*/ 0u, clock_.CurrentTime());
    uint64_t transport_sequence_number =
        UseTransportLayerFeedback() ? packet.transport_sequence_number
                                    : packet.transport_sequence_number & 0xFFFF;
    adapter.ProcessSentPacket(
        rtc::SentPacket(transport_sequence_number, packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback;
  if (!UseTransportLayerFeedback()) {
    rtcp::TransportFeedback rtcp = BuildRtcpTransportFeedbackPacket(packets);
    adapted_feedback =
        adapter.ProcessTransportFeedback(rtcp, clock_.CurrentTime());
  } else {
    rtcp::CongestionControlFeedback rtcp =
        BuildRtcpCongestionControlFeedbackPacket(packets);
    adapted_feedback =
        adapter.ProcessCongestionControlFeedback(rtcp, clock_.CurrentTime());
  }

  ComparePacketFeedbackVectors(packets, adapted_feedback->packet_feedbacks);
}

TEST_P(TransportFeedbackAdapterTest, FeedbackVectorReportsUnreceived) {
  TransportFeedbackAdapter adapter;

  std::vector<PacketTemplate> packets = {
      {
          .transport_sequence_number = 1,
          .rtp_sequence_number = 101,
          .send_timestamp = Timestamp::Millis(200),
          .receive_timestamp = Timestamp::Millis(100),
      },
      {
          .transport_sequence_number = 2,
          .rtp_sequence_number = 102,
          .send_timestamp = Timestamp::Millis(210),
          .receive_timestamp =
              Timestamp::MinusInfinity()  // Packet not received.
      },
      {
          .transport_sequence_number = 3,
          .rtp_sequence_number = 103,
          .send_timestamp = Timestamp::Millis(220),
          .receive_timestamp = Timestamp::Millis(110),
      },
      {
          .transport_sequence_number = 4,
          .rtp_sequence_number = 104,
          .send_timestamp = Timestamp::Millis(230),
          .receive_timestamp =
              Timestamp::MinusInfinity()  // Packet not received.
      },
      {
          .transport_sequence_number = 5,
          .rtp_sequence_number = 105,
          .send_timestamp = Timestamp::Millis(240),
          .receive_timestamp = Timestamp::Millis(120),
      },
  };

  for (const auto& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet, !UseTransportLayerFeedback()),
                      packet.pacing_info,
                      /*overhead*/ 0u, clock_.CurrentTime());
    uint64_t transport_sequence_number =
        UseTransportLayerFeedback() ? packet.transport_sequence_number
                                    : packet.transport_sequence_number & 0xFFFF;
    adapter.ProcessSentPacket(
        rtc::SentPacket(transport_sequence_number, packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback;
  if (!UseTransportLayerFeedback()) {
    rtcp::TransportFeedback rtcp = BuildRtcpTransportFeedbackPacket(packets);
    adapted_feedback =
        adapter.ProcessTransportFeedback(rtcp, clock_.CurrentTime());
  } else {
    rtcp::CongestionControlFeedback rtcp =
        BuildRtcpCongestionControlFeedbackPacket(packets);
    adapted_feedback =
        adapter.ProcessCongestionControlFeedback(rtcp, clock_.CurrentTime());
  }

  ComparePacketFeedbackVectors(packets, adapted_feedback->packet_feedbacks);
}

// RFC8888, Also test lost between feedbacks? How do we know if feedback packet
// is lost?
// ...to count lost feedback packets? Should we instead change of feedback to
// remember

TEST_P(TransportFeedbackAdapterTest, HandlesArrivalReordering) {
  TransportFeedbackAdapter adapter;

  std::vector<PacketTemplate> packets = {
      {
          .transport_sequence_number = 1,
          .rtp_sequence_number = 101,
          .send_timestamp = Timestamp::Millis(200),
          .receive_timestamp = Timestamp::Millis(100),
      },
      {
          .transport_sequence_number = 2,
          .rtp_sequence_number = 102,
          .send_timestamp = Timestamp::Millis(210),
          .receive_timestamp = Timestamp::Millis(90),
      },
      {
          .transport_sequence_number = 3,
          .rtp_sequence_number = 103,
          .send_timestamp = Timestamp::Millis(220),
          .receive_timestamp = Timestamp::Millis(70),
      }};

  for (const auto& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet, !UseTransportLayerFeedback()),
                      packet.pacing_info,
                      /*overhead*/ 0u, clock_.CurrentTime());
    uint64_t transport_sequence_number =
        UseTransportLayerFeedback() ? packet.transport_sequence_number
                                    : packet.transport_sequence_number & 0xFFFF;
    adapter.ProcessSentPacket(
        rtc::SentPacket(transport_sequence_number, packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback;
  if (!UseTransportLayerFeedback()) {
    rtcp::TransportFeedback rtcp = BuildRtcpTransportFeedbackPacket(packets);
    adapted_feedback =
        adapter.ProcessTransportFeedback(rtcp, clock_.CurrentTime());
  } else {
    rtcp::CongestionControlFeedback rtcp =
        BuildRtcpCongestionControlFeedbackPacket(packets);
    adapted_feedback =
        adapter.ProcessCongestionControlFeedback(rtcp, clock_.CurrentTime());
  }

  // Adapter keeps the packets ordered by sequence number (which is itself
  // assigned by the order of transmission). Reordering by some other criteria,
  // eg. arrival time, is up to the observers.
  ComparePacketFeedbackVectors(packets, adapted_feedback->packet_feedbacks);
}

TEST_F(TransportFeedbackAdapterTest, TimestampDeltas) {
  std::vector<PacketResult> sent_packets;
  // TODO(srte): Consider using us resolution in the constants.
  const TimeDelta kSmallDelta = (rtcp::TransportFeedback::kDeltaTick * 0xFF)
                                    .RoundDownTo(TimeDelta::Millis(1));
  const TimeDelta kLargePositiveDelta = (rtcp::TransportFeedback::kDeltaTick *
                                         std::numeric_limits<int16_t>::max())
                                            .RoundDownTo(TimeDelta::Millis(1));
  const TimeDelta kLargeNegativeDelta = (rtcp::TransportFeedback::kDeltaTick *
                                         std::numeric_limits<int16_t>::min())
                                            .RoundDownTo(TimeDelta::Millis(1));

  PacketResult packet_feedback;
  packet_feedback.sent_packet.sequence_number = 1;
  packet_feedback.sent_packet.send_time = Timestamp::Millis(100);
  packet_feedback.receive_time = Timestamp::Millis(200);
  packet_feedback.sent_packet.size = DataSize::Bytes(1500);
  sent_packets.push_back(packet_feedback);

  // TODO(srte): This rounding maintains previous behavior, but should ot be
  // required.
  packet_feedback.sent_packet.send_time += kSmallDelta;
  packet_feedback.receive_time += kSmallDelta;
  ++packet_feedback.sent_packet.sequence_number;
  sent_packets.push_back(packet_feedback);

  packet_feedback.sent_packet.send_time += kLargePositiveDelta;
  packet_feedback.receive_time += kLargePositiveDelta;
  ++packet_feedback.sent_packet.sequence_number;
  sent_packets.push_back(packet_feedback);

  packet_feedback.sent_packet.send_time += kLargeNegativeDelta;
  packet_feedback.receive_time += kLargeNegativeDelta;
  ++packet_feedback.sent_packet.sequence_number;
  sent_packets.push_back(packet_feedback);

  // Too large, delta - will need two feedback messages.
  packet_feedback.sent_packet.send_time +=
      kLargePositiveDelta + TimeDelta::Millis(1);
  packet_feedback.receive_time += kLargePositiveDelta + TimeDelta::Millis(1);
  ++packet_feedback.sent_packet.sequence_number;

  // Packets will be added to send history.
  for (const auto& packet : sent_packets)
    OnSentPacket(packet);
  OnSentPacket(packet_feedback);

  // Create expected feedback and send into adapter.
  std::unique_ptr<rtcp::TransportFeedback> feedback(
      new rtcp::TransportFeedback());
  feedback->SetBase(sent_packets[0].sent_packet.sequence_number,
                    sent_packets[0].receive_time);

  for (const auto& packet : sent_packets) {
    EXPECT_TRUE(feedback->AddReceivedPacket(packet.sent_packet.sequence_number,
                                            packet.receive_time));
  }
  EXPECT_FALSE(
      feedback->AddReceivedPacket(packet_feedback.sent_packet.sequence_number,
                                  packet_feedback.receive_time));

  rtc::Buffer raw_packet = feedback->Build();
  feedback =
      rtcp::TransportFeedback::ParseFrom(raw_packet.data(), raw_packet.size());

  std::vector<PacketResult> received_feedback;

  ASSERT_TRUE(feedback.get() != nullptr);
  auto res =
      adapter_->ProcessTransportFeedback(*feedback.get(), clock_.CurrentTime());
  ComparePacketFeedbackVectors(sent_packets, res->packet_feedbacks);

  // Create a new feedback message and add the trailing item.
  feedback.reset(new rtcp::TransportFeedback());
  feedback->SetBase(packet_feedback.sent_packet.sequence_number,
                    packet_feedback.receive_time);
  EXPECT_TRUE(
      feedback->AddReceivedPacket(packet_feedback.sent_packet.sequence_number,
                                  packet_feedback.receive_time));
  raw_packet = feedback->Build();
  feedback =
      rtcp::TransportFeedback::ParseFrom(raw_packet.data(), raw_packet.size());

  ASSERT_TRUE(feedback.get() != nullptr);
  {
    auto res = adapter_->ProcessTransportFeedback(*feedback.get(),
                                                  clock_.CurrentTime());
    std::vector<PacketResult> expected_packets;
    expected_packets.push_back(packet_feedback);
    ComparePacketFeedbackVectors(expected_packets, res->packet_feedbacks);
  }
}

TEST_P(TransportFeedbackAdapterTest, IgnoreDuplicatePacketSentCalls) {
  auto packet = CreatePacket(100, 200, 0, 1500, kPacingInfo0);

  // Add a packet and then mark it as sent.
  RtpPacketSendInfo packet_info;
  packet_info.media_ssrc = kSsrc;
  packet_info.transport_sequence_number = packet.sent_packet.sequence_number;
  packet_info.length = packet.sent_packet.size.bytes();
  packet_info.pacing_info = packet.sent_packet.pacing_info;
  packet_info.packet_type = RtpPacketMediaType::kVideo;
  adapter_->AddPacket(packet_info, 0u, clock_.CurrentTime());
  std::optional<SentPacket> sent_packet = adapter_->ProcessSentPacket(
      rtc::SentPacket(packet.sent_packet.sequence_number,
                      packet.sent_packet.send_time.ms(), rtc::PacketInfo()));
  EXPECT_TRUE(sent_packet.has_value());

  // Call ProcessSentPacket() again with the same sequence number. This packet
  // has already been marked as sent and the call should be ignored.
  std::optional<SentPacket> duplicate_packet = adapter_->ProcessSentPacket(
      rtc::SentPacket(packet.sent_packet.sequence_number,
                      packet.sent_packet.send_time.ms(), rtc::PacketInfo()));
  EXPECT_FALSE(duplicate_packet.has_value());
}

TEST_P(TransportFeedbackAdapterTest,
       SendReceiveTimeDiffTimeContinuouseBetweenFeedback) {
  TransportFeedbackAdapter adapter;

  std::vector<PacketTemplate> packets = {
      {
          .transport_sequence_number = 1,
          .rtp_sequence_number = 101,
          .send_timestamp = Timestamp::Millis(100),
          .pacing_info = kPacingInfo0,
          .receive_timestamp = Timestamp::Millis(200),
      },
      {
          .transport_sequence_number = 2,
          .rtp_sequence_number = 102,
          .send_timestamp = Timestamp::Millis(110),
          .pacing_info = kPacingInfo0,
          .receive_timestamp = Timestamp::Millis(210),
      }};

  for (const auto& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet, !UseTransportLayerFeedback()),
                      packet.pacing_info,
                      /*overhead*/ 0u, clock_.CurrentTime());

    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback_1;
  std::optional<TransportPacketsFeedback> adapted_feedback_2;
  if (!UseTransportLayerFeedback()) {
    adapted_feedback_1 = adapter.ProcessTransportFeedback(
        BuildRtcpTransportFeedbackPacket({packets[0]}),
        /*feedback_receive_time=*/clock_.CurrentTime());
    adapted_feedback_2 = adapter.ProcessTransportFeedback(
        BuildRtcpTransportFeedbackPacket({packets[1]}),
        /*feedback_receive_time=*/clock_.CurrentTime());
  } else {
    rtcp::CongestionControlFeedback rtcp =
        BuildRtcpCongestionControlFeedbackPacket({packets[0]});
    adapted_feedback_1 = adapter.ProcessCongestionControlFeedback(
        BuildRtcpCongestionControlFeedbackPacket({packets[0]}),
        /*feedback_receive_time=*/clock_.CurrentTime());
    adapted_feedback_2 = adapter.ProcessCongestionControlFeedback(
        BuildRtcpCongestionControlFeedbackPacket({packets[1]}),
        /*feedback_receive_time=*/clock_.CurrentTime());
  }

  ASSERT_EQ(adapted_feedback_1->packet_feedbacks.size(),
            adapted_feedback_2->packet_feedbacks.size());
  ASSERT_THAT(adapted_feedback_1->packet_feedbacks, testing::SizeIs(1));
  EXPECT_EQ((adapted_feedback_1->packet_feedbacks[0].receive_time -
             adapted_feedback_1->packet_feedbacks[0].sent_packet.send_time)
                .RoundTo(TimeDelta::Millis(1)),
            (adapted_feedback_2->packet_feedbacks[0].receive_time -
             adapted_feedback_2->packet_feedbacks[0].sent_packet.send_time)
                .RoundTo(TimeDelta::Millis(1)));
}

TEST_F(TransportFeedbackAdapterTest, ProcessSentPacketIncreaseOutstandingData) {
  TransportFeedbackAdapter adapter;

  PacketTemplate packet_1 = {.transport_sequence_number = 1,
                             .packet_size = DataSize::Bytes(200)};
  PacketTemplate packet_2 = {.transport_sequence_number = 2,
                             .packet_size = DataSize::Bytes(300)};
  adapter.AddPacket(
      CreatePacketToSend(packet_1, /*set_header_extensions=*/false),
      packet_1.pacing_info,
      /*overhead*/ 0u, clock_.CurrentTime());
  std::optional<SentPacket> sent_packet_1 =
      adapter.ProcessSentPacket(rtc::SentPacket(
          packet_1.transport_sequence_number, packet_1.send_timestamp.ms()));

  ASSERT_TRUE(sent_packet_1.has_value());
  EXPECT_EQ(sent_packet_1->sequence_number, packet_1.transport_sequence_number);
  // Only one packet in flight.
  EXPECT_EQ(sent_packet_1->data_in_flight, packet_1.packet_size);
  EXPECT_EQ(adapter.GetOutstandingData(), packet_1.packet_size);

  adapter.AddPacket(
      CreatePacketToSend(packet_2, /*set_header_extensions=*/false),
      packet_2.pacing_info,
      /*overhead*/ 0u, clock_.CurrentTime());
  std::optional<SentPacket> sent_packet_2 =
      adapter.ProcessSentPacket(rtc::SentPacket(
          packet_2.transport_sequence_number, packet_2.send_timestamp.ms()));

  ASSERT_TRUE(sent_packet_2.has_value());
  // Two packets in flight.
  EXPECT_EQ(sent_packet_2->data_in_flight,
            packet_1.packet_size + packet_2.packet_size);

  EXPECT_EQ(adapter.GetOutstandingData(),
            packet_1.packet_size + packet_2.packet_size);
}

TEST_P(TransportFeedbackAdapterTest, TransportPacketFeedbackHasDataInFlight) {
  TransportFeedbackAdapter adapter;

  std::vector<PacketTemplate> packets = {
      {
          .transport_sequence_number = 1,
          .rtp_sequence_number = 101,
          .packet_size = DataSize::Bytes(200),
          .send_timestamp = Timestamp::Millis(100),
          .pacing_info = kPacingInfo0,
          .receive_timestamp = Timestamp::Millis(200),
      },
      {
          .transport_sequence_number = 2,
          .rtp_sequence_number = 102,
          .packet_size = DataSize::Bytes(300),
          .send_timestamp = Timestamp::Millis(110),
          .pacing_info = kPacingInfo0,
          .receive_timestamp = Timestamp::Millis(210),
      }};

  for (const auto& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet, !UseTransportLayerFeedback()),
                      packet.pacing_info,
                      /*overhead*/ 0u, clock_.CurrentTime());

    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback_1;
  std::optional<TransportPacketsFeedback> adapted_feedback_2;
  if (!UseTransportLayerFeedback()) {
    adapted_feedback_1 = adapter.ProcessTransportFeedback(
        BuildRtcpTransportFeedbackPacket({packets[0]}),
        /*feedback_receive_time=*/clock_.CurrentTime());
    adapted_feedback_2 = adapter.ProcessTransportFeedback(
        BuildRtcpTransportFeedbackPacket({packets[1]}),
        /*feedback_receive_time=*/clock_.CurrentTime());
  } else {
    rtcp::CongestionControlFeedback rtcp =
        BuildRtcpCongestionControlFeedbackPacket({packets[0]});
    adapted_feedback_1 = adapter.ProcessCongestionControlFeedback(
        BuildRtcpCongestionControlFeedbackPacket({packets[0]}),
        /*feedback_receive_time=*/clock_.CurrentTime());
    adapted_feedback_2 = adapter.ProcessCongestionControlFeedback(
        BuildRtcpCongestionControlFeedbackPacket({packets[1]}),
        /*feedback_receive_time=*/clock_.CurrentTime());
  }

  EXPECT_EQ(adapted_feedback_1->data_in_flight, packets[1].packet_size);
  EXPECT_EQ(adapted_feedback_2->data_in_flight, DataSize::Zero());
}

}  // namespace webrtc
