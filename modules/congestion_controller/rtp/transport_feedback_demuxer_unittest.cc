/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/rtp/transport_feedback_demuxer.h"

#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using PacketInfo = StreamFeedbackObserver::StreamPacketInfo;
static constexpr uint32_t kSsrc = 8492;

class MockStreamFeedbackObserver : public webrtc::StreamFeedbackObserver {
 public:
  MOCK_METHOD(void,
              OnPacketFeedbackVector,
              (std::vector<StreamPacketInfo> packet_feedback_vector),
              (override));
};

RtpPacketSendInfo CreatePacket(uint32_t ssrc,
                               int16_t rtp_sequence_number,
                               int64_t transport_sequence_number,
                               bool is_retransmission) {
  RtpPacketSendInfo res;
  res.ssrc = ssrc;
  res.transport_sequence_number = transport_sequence_number;
  res.rtp_sequence_number = rtp_sequence_number;
  res.packet_type = is_retransmission ? RtpPacketMediaType::kRetransmission
                                      : RtpPacketMediaType::kVideo;
  return res;
}
}  // namespace

TEST(TransportFeedbackDemuxerTest, ObserverSanity) {
  TransportFeedbackDemuxer demuxer;
  MockStreamFeedbackObserver mock;
  demuxer.RegisterStreamFeedbackObserver({kSsrc}, &mock);

  demuxer.AddPacket(CreatePacket(kSsrc, 55, 1, false));
  demuxer.AddPacket(CreatePacket(kSsrc, 56, 2, false));
  demuxer.AddPacket(CreatePacket(kSsrc, 57, 3, true));

  rtcp::TransportFeedback feedback;
  feedback.SetBase(1, 1000);
  ASSERT_TRUE(feedback.AddReceivedPacket(1, 1000));
  // Drop packet with transport seq 2.
  ASSERT_TRUE(feedback.AddReceivedPacket(3, 3000));

  EXPECT_CALL(mock, OnPacketFeedbackVector(ElementsAre(
                        AllOf(Field(&PacketInfo::received, true),
                              Field(&PacketInfo::ssrc, kSsrc),
                              Field(&PacketInfo::rtp_sequence_number, 55),
                              Field(&PacketInfo::is_retransmission, false)),
                        AllOf(Field(&PacketInfo::received, false),
                              Field(&PacketInfo::ssrc, kSsrc),
                              Field(&PacketInfo::rtp_sequence_number, 56),
                              Field(&PacketInfo::is_retransmission, false)),
                        AllOf(Field(&PacketInfo::received, true),
                              Field(&PacketInfo::ssrc, kSsrc),
                              Field(&PacketInfo::rtp_sequence_number, 57),
                              Field(&PacketInfo::is_retransmission, true)))));
  demuxer.OnTransportFeedback(feedback);

  demuxer.DeRegisterStreamFeedbackObserver(&mock);

  demuxer.AddPacket(CreatePacket(kSsrc, 58, 4, false));
  rtcp::TransportFeedback second_feedback;
  second_feedback.SetBase(4, 4000);
  ASSERT_TRUE(second_feedback.AddReceivedPacket(4, 4000));

  EXPECT_CALL(mock, OnPacketFeedbackVector).Times(0);
  demuxer.OnTransportFeedback(second_feedback);
}
}  // namespace webrtc
