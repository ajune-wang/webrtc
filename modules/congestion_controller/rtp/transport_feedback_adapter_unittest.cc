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

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/transport/network_types.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/ntp_time_util.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/network/ecn_marking.h"
#include "rtc_base/network/sent_packet.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace webrtc {
namespace {

using ::testing::SizeIs;

constexpr uint32_t kSsrc = 8492;
const PacedPacketInfo kPacingInfo0(0, 5, 2000);
const PacedPacketInfo kPacingInfo1(1, 8, 4000);
const PacedPacketInfo kPacingInfo2(2, 14, 7000);
const PacedPacketInfo kPacingInfo3(3, 20, 10000);
const PacedPacketInfo kPacingInfo4(4, 22, 10000);

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

  bool is_audio = false;
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

RtpPacketToSend CreatePacketToSend(PacketTemplate packet) {
  RtpPacketToSend send_packet(nullptr);
  send_packet.SetSsrc(packet.ssrc);
  send_packet.SetPayloadSize(packet.packet_size.bytes() -
                             send_packet.headers_size());
  send_packet.SetSequenceNumber(packet.rtp_sequence_number);
  send_packet.set_transport_sequence_number(packet.transport_sequence_number);
  send_packet.set_packet_type(packet.is_audio ? RtpPacketMediaType::kAudio
                                              : RtpPacketMediaType::kVideo);

  return send_packet;
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
           .arrival_time_offset = feedback_sent_time - packet.receive_timestamp,
           .ecn = packet.ecn});
    }
  }

  SimulatedClock clock(feedback_sent_time);
  uint32_t compact_ntp =
      CompactNtp(clock.ConvertTimestampToNtpTime(feedback_sent_time));
  return rtcp::CongestionControlFeedback(std::move(packet_infos), compact_ntp);
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
  TransportFeedbackAdapterTest() {}

  bool UseRfc8888CongestionControlFeedback() const { return GetParam(); }

  Timestamp TimeNow() const { return Timestamp::Millis(1234); }

  std::optional<TransportPacketsFeedback> CreateAndProcessFeedback(
      TransportFeedbackAdapter& adapter,
      const std::vector<PacketTemplate>& packets) {
    if (UseRfc8888CongestionControlFeedback()) {
      rtcp::CongestionControlFeedback rtcp_feedback =
          BuildRtcpCongestionControlFeedbackPacket(packets);
      return adapter.ProcessCongestionControlFeedback(rtcp_feedback, TimeNow());
    } else {
      rtcp::TransportFeedback rtcp_feedback =
          BuildRtcpTransportFeedbackPacket(packets);
      return adapter.ProcessTransportFeedback(rtcp_feedback, TimeNow());
    }
  }
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
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead*/ 0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(adapter, packets);
  ComparePacketFeedbackVectors(packets, adapted_feedback->packet_feedbacks);
}

TEST_P(TransportFeedbackAdapterTest, FeedbackVectorReportLostPackets) {
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
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead*/ 0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(adapter, packets);
  ComparePacketFeedbackVectors(packets, adapted_feedback->packet_feedbacks);
}

TEST_P(TransportFeedbackAdapterTest, FeedbackReportsIfPacketIsAudio) {
  std::vector<PacketTemplate> packets = {
      {.ssrc = 1,
       .transport_sequence_number = 1,
       .rtp_sequence_number = 101,
       .send_timestamp = Timestamp::Millis(200),
       .receive_timestamp = Timestamp::Millis(100),
       .is_audio = true},
      {.ssrc = 2,
       .transport_sequence_number = 2,
       .rtp_sequence_number = 102,
       .send_timestamp = Timestamp::Millis(200),
       .receive_timestamp = Timestamp::Millis(100),
       .is_audio = false}};

  TransportFeedbackAdapter adapter;
  for (const auto& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead*/ 0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }
  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(adapter, packets);

  ASSERT_THAT(adapted_feedback->packet_feedbacks, SizeIs(2));
  EXPECT_EQ(adapted_feedback->packet_feedbacks[0].sent_packet.sequence_number,
            1);
  EXPECT_TRUE(adapted_feedback->packet_feedbacks[0].sent_packet.audio);
  EXPECT_EQ(adapted_feedback->packet_feedbacks[1].sent_packet.sequence_number,
            2);
  EXPECT_FALSE(adapted_feedback->packet_feedbacks[1].sent_packet.audio);
}

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
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead*/ 0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(adapter, packets);

  // Adapter keeps the packets ordered by sequence number (which is itself
  // assigned by the order of transmission). Reordering by some other criteria,
  // eg. arrival time, is up to the observers.
  ComparePacketFeedbackVectors(packets, adapted_feedback->packet_feedbacks);
}

TEST_P(TransportFeedbackAdapterTest, IgnoreDuplicatePacketSentCalls) {
  TransportFeedbackAdapter adapter;
  PacketTemplate packet = {
      .ssrc = kSsrc,
      .transport_sequence_number = 1,
      .rtp_sequence_number = 101,
      .send_timestamp = Timestamp::Millis(200),
      .receive_timestamp = Timestamp::Millis(100),
  };
  RtpPacketToSend packet_to_send = CreatePacketToSend(packet);
  // Add a packet and then mark it as sent.
  adapter.AddPacket(packet_to_send, PacedPacketInfo(), 0u, TimeNow());
  std::optional<SentPacket> sent_packet = adapter.ProcessSentPacket(
      rtc::SentPacket(packet.transport_sequence_number,
                      packet.send_timestamp.ms(), rtc::PacketInfo()));
  EXPECT_TRUE(sent_packet.has_value());

  // Call ProcessSentPacket() again with the same sequence number. This packet
  // has already been marked as sent and the call should be ignored.
  std::optional<SentPacket> duplicate_packet = adapter.ProcessSentPacket(
      rtc::SentPacket(packet.transport_sequence_number,
                      packet.send_timestamp.ms(), rtc::PacketInfo()));
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
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead*/ 0u, TimeNow());

    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback_1 =
      CreateAndProcessFeedback(adapter, {packets[0]});
  std::optional<TransportPacketsFeedback> adapted_feedback_2 =
      CreateAndProcessFeedback(adapter, {packets[1]});

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
  adapter.AddPacket(CreatePacketToSend(packet_1), packet_1.pacing_info,
                    /*overhead*/ 0u, TimeNow());
  std::optional<SentPacket> sent_packet_1 =
      adapter.ProcessSentPacket(rtc::SentPacket(
          packet_1.transport_sequence_number, packet_1.send_timestamp.ms()));

  ASSERT_TRUE(sent_packet_1.has_value());
  EXPECT_EQ(sent_packet_1->sequence_number, packet_1.transport_sequence_number);
  // Only one packet in flight.
  EXPECT_EQ(sent_packet_1->data_in_flight, packet_1.packet_size);
  EXPECT_EQ(adapter.GetOutstandingData(), packet_1.packet_size);

  adapter.AddPacket(CreatePacketToSend(packet_2), packet_2.pacing_info,
                    /*overhead*/ 0u, TimeNow());
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
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead*/ 0u, TimeNow());

    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback_1 =
      CreateAndProcessFeedback(adapter, {packets[0]});
  std::optional<TransportPacketsFeedback> adapted_feedback_2 =
      CreateAndProcessFeedback(adapter, {packets[1]});
  EXPECT_EQ(adapted_feedback_1->data_in_flight, packets[1].packet_size);
  EXPECT_EQ(adapted_feedback_2->data_in_flight, DataSize::Zero());
}

TEST_F(TransportFeedbackAdapterTest, CongestionControlFeedbackResultHasEcn) {
  TransportFeedbackAdapter adapter;

  PacketTemplate packet = {
      .transport_sequence_number = 1,
      .rtp_sequence_number = 101,
      .packet_size = DataSize::Bytes(200),
      .send_timestamp = Timestamp::Millis(100),
      .pacing_info = kPacingInfo0,
      .receive_timestamp = Timestamp::Millis(200),
  };

  adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                    /*overhead*/ 0u, TimeNow());
  adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                            packet.send_timestamp.ms()));

  packet.ecn = rtc::EcnMarking::kCe;
  rtcp::CongestionControlFeedback rtcp_feedback =
      BuildRtcpCongestionControlFeedbackPacket({packet});
  std::optional<TransportPacketsFeedback> adapted_feedback =
      adapter.ProcessCongestionControlFeedback(rtcp_feedback, TimeNow());

  ASSERT_THAT(adapted_feedback->packet_feedbacks, SizeIs(1));
  ASSERT_THAT(adapted_feedback->packet_feedbacks[0].ecn, rtc::EcnMarking::kCe);
}

}  // namespace webrtc
