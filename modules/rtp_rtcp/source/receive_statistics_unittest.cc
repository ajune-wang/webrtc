/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
const size_t kPacketSize1 = 100;
const size_t kPacketSize2 = 300;
const uint32_t kSsrc1 = 1;
const uint32_t kSsrc2 = 2;

std::unique_ptr<RtpPacketReceived> CreatePacketWithSize(uint32_t ssrc,
                                                        size_t packet_size) {
  auto packet = rtc::MakeUnique<RtpPacketReceived>();
  packet->SetSsrc(ssrc);
  packet->SetPayloadSize(packet_size - 12);
  packet->set_payload_type_frequency(kVideoPayloadTypeFrequency);
  return packet;
}

std::unique_ptr<RtpPacketReceived> CreatePacketWithPayloadAndExtraHeaders(
    uint32_t ssrc,
    size_t payload_size) {
  auto packet = rtc::MakeUnique<RtpPacketReceived>();
  packet->SetCsrcs({5, 4, 3});  // Increase header size.
  EXPECT_GT(packet->headers_size(), 12u);

  packet->SetSsrc(ssrc);
  packet->SetPayloadSize(payload_size);
  packet->set_payload_type_frequency(kVideoPayloadTypeFrequency);
  return packet;
}

void IncrementSequenceNumber(RtpPacketReceived* packet, uint16_t incr = 1) {
  packet->SetSequenceNumber(packet->SequenceNumber() + incr);
}

void IncrementTimestame(RtpPacketReceived* packet, uint32_t incr) {
  packet->SetTimestamp(packet->Timestamp() + incr);
}

}  // namespace

class ReceiveStatisticsTest : public ::testing::Test {
 public:
  ReceiveStatisticsTest()
      : clock_(0), receive_statistics_(ReceiveStatistics::Create(&clock_)) {}

 protected:
  SimulatedClock clock_;
  std::unique_ptr<ReceiveStatistics> receive_statistics_;
};

TEST_F(ReceiveStatisticsTest, TwoIncomingSsrcs) {
  std::unique_ptr<RtpPacketReceived> packet1 =
      CreatePacketWithSize(kSsrc1, kPacketSize1);
  std::unique_ptr<RtpPacketReceived> packet2 =
      CreatePacketWithSize(kSsrc2, kPacketSize2);
  receive_statistics_->OnRtpPacket(*packet1);
  IncrementSequenceNumber(packet1.get());
  receive_statistics_->OnRtpPacket(*packet2);
  IncrementSequenceNumber(packet2.get());
  clock_.AdvanceTimeMilliseconds(100);
  receive_statistics_->OnRtpPacket(*packet1);
  IncrementSequenceNumber(packet1.get());
  receive_statistics_->OnRtpPacket(*packet2);
  IncrementSequenceNumber(packet2.get());

  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);
  EXPECT_GT(statistician->BitrateReceived(), 0u);
  size_t bytes_received = 0;
  uint32_t packets_received = 0;
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(200u, bytes_received);
  EXPECT_EQ(2u, packets_received);

  statistician =
      receive_statistics_->GetStatistician(kSsrc2);
  ASSERT_TRUE(statistician != NULL);
  EXPECT_GT(statistician->BitrateReceived(), 0u);
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(600u, bytes_received);
  EXPECT_EQ(2u, packets_received);

  EXPECT_EQ(2u, receive_statistics_->RtcpReportBlocks(3).size());
  // Add more incoming packets and verify that they are registered in both
  // access methods.
  receive_statistics_->OnRtpPacket(*packet1);
  IncrementSequenceNumber(packet1.get());
  receive_statistics_->OnRtpPacket(*packet2);
  IncrementSequenceNumber(packet2.get());

  receive_statistics_->GetStatistician(kSsrc1)->GetDataCounters(
      &bytes_received, &packets_received);
  EXPECT_EQ(300u, bytes_received);
  EXPECT_EQ(3u, packets_received);
  receive_statistics_->GetStatistician(kSsrc2)->GetDataCounters(
      &bytes_received, &packets_received);
  EXPECT_EQ(900u, bytes_received);
  EXPECT_EQ(3u, packets_received);
}

TEST_F(ReceiveStatisticsTest, ActiveStatisticians) {
  std::unique_ptr<RtpPacketReceived> packet1 =
      CreatePacketWithSize(kSsrc1, kPacketSize1);
  std::unique_ptr<RtpPacketReceived> packet2 =
      CreatePacketWithSize(kSsrc2, kPacketSize2);
  receive_statistics_->OnRtpPacket(*packet1);
  IncrementSequenceNumber(packet1.get(), 1);
  clock_.AdvanceTimeMilliseconds(1000);
  receive_statistics_->OnRtpPacket(*packet2);
  IncrementSequenceNumber(packet2.get());
  // Nothing should time out since only 1000 ms has passed since the first
  // packet came in.
  EXPECT_EQ(2u, receive_statistics_->RtcpReportBlocks(3).size());

  clock_.AdvanceTimeMilliseconds(7000);
  // kSsrc1 should have timed out.
  EXPECT_EQ(1u, receive_statistics_->RtcpReportBlocks(3).size());

  clock_.AdvanceTimeMilliseconds(1000);
  // kSsrc2 should have timed out.
  EXPECT_EQ(0u, receive_statistics_->RtcpReportBlocks(3).size());

  receive_statistics_->OnRtpPacket(*packet1);
  IncrementSequenceNumber(packet1.get());
  // kSsrc1 should be active again and the data counters should have survived.
  EXPECT_EQ(1u, receive_statistics_->RtcpReportBlocks(3).size());
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);
  size_t bytes_received = 0;
  uint32_t packets_received = 0;
  statistician->GetDataCounters(&bytes_received, &packets_received);
  EXPECT_EQ(200u, bytes_received);
  EXPECT_EQ(2u, packets_received);
}

TEST_F(ReceiveStatisticsTest, GetReceiveStreamDataCounters) {
  std::unique_ptr<RtpPacketReceived> packet =
      CreatePacketWithSize(kSsrc1, kPacketSize1);
  receive_statistics_->OnRtpPacket(*packet);
  StreamStatistician* statistician =
      receive_statistics_->GetStatistician(kSsrc1);
  ASSERT_TRUE(statistician != NULL);

  StreamDataCounters counters;
  statistician->GetReceiveStreamDataCounters(&counters);
  EXPECT_GT(counters.first_packet_time_ms, -1);
  EXPECT_EQ(1u, counters.transmitted.packets);

  receive_statistics_->OnRtpPacket(*packet);
  statistician->GetReceiveStreamDataCounters(&counters);
  EXPECT_GT(counters.first_packet_time_ms, -1);
  EXPECT_EQ(2u, counters.transmitted.packets);
}

TEST_F(ReceiveStatisticsTest, RtcpCallbacks) {
  class TestCallback : public RtcpStatisticsCallback {
   public:
    TestCallback()
        : RtcpStatisticsCallback(), num_calls_(0), ssrc_(0), stats_() {}
    virtual ~TestCallback() {}

    void StatisticsUpdated(const RtcpStatistics& statistics,
                           uint32_t ssrc) override {
      ssrc_ = ssrc;
      stats_ = statistics;
      ++num_calls_;
    }

    void CNameChanged(const char* cname, uint32_t ssrc) override {}

    uint32_t num_calls_;
    uint32_t ssrc_;
    RtcpStatistics stats_;
  } callback;

  receive_statistics_->RegisterRtcpStatisticsCallback(&callback);

  // Add some arbitrary data, with loss and jitter.
  std::unique_ptr<RtpPacketReceived> packet =
      CreatePacketWithSize(kSsrc1, kPacketSize1);
  packet->SetSequenceNumber(1);
  clock_.AdvanceTimeMilliseconds(7);
  IncrementTimestame(packet.get(), 3);
  receive_statistics_->OnRtpPacket(*packet);
  IncrementSequenceNumber(packet.get(), 2);
  clock_.AdvanceTimeMilliseconds(9);
  IncrementTimestame(packet.get(), 9);
  receive_statistics_->OnRtpPacket(*packet);
  IncrementSequenceNumber(packet.get(), -1);
  clock_.AdvanceTimeMilliseconds(13);
  IncrementTimestame(packet.get(), 47);
  receive_statistics_->OnRtpPacket(*packet);  // XXX retransmitted == true
  IncrementSequenceNumber(packet.get(), 3);
  clock_.AdvanceTimeMilliseconds(11);
  IncrementTimestame(packet.get(), 17);
  receive_statistics_->OnRtpPacket(*packet);
  IncrementSequenceNumber(packet.get(), 1);

  EXPECT_EQ(0u, callback.num_calls_);

  // Call GetStatistics, simulating a timed rtcp sender thread.
  RtcpStatistics statistics;
  receive_statistics_->GetStatistician(kSsrc1)
      ->GetStatistics(&statistics, true);

  EXPECT_EQ(1u, callback.num_calls_);
  EXPECT_EQ(callback.ssrc_, kSsrc1);
  EXPECT_EQ(statistics.packets_lost, callback.stats_.packets_lost);
  EXPECT_EQ(statistics.extended_highest_sequence_number,
            callback.stats_.extended_highest_sequence_number);
  EXPECT_EQ(statistics.fraction_lost, callback.stats_.fraction_lost);
  EXPECT_EQ(statistics.jitter, callback.stats_.jitter);
  EXPECT_EQ(51, statistics.fraction_lost);
  EXPECT_EQ(1u, statistics.packets_lost);
  EXPECT_EQ(5u, statistics.extended_highest_sequence_number);
  EXPECT_EQ(4u, statistics.jitter);

  receive_statistics_->RegisterRtcpStatisticsCallback(NULL);

  // Add some more data.
  packet->SetSequenceNumber(1);
  clock_.AdvanceTimeMilliseconds(7);
  IncrementTimestame(packet.get(), 3);
  receive_statistics_->OnRtpPacket(*packet);
  IncrementSequenceNumber(packet.get(), 2);
  clock_.AdvanceTimeMilliseconds(9);
  IncrementTimestame(packet.get(), 9);
  receive_statistics_->OnRtpPacket(*packet);
  IncrementSequenceNumber(packet.get(), -1);
  clock_.AdvanceTimeMilliseconds(13);
  IncrementTimestame(packet.get(), 47);
  receive_statistics_->OnRtpPacket(*packet);  // XXX retransmitted == true
  IncrementSequenceNumber(packet.get(), 3);
  clock_.AdvanceTimeMilliseconds(11);
  IncrementTimestame(packet.get(), 17);
  receive_statistics_->OnRtpPacket(*packet);
  IncrementSequenceNumber(packet.get());

  receive_statistics_->GetStatistician(kSsrc1)
      ->GetStatistics(&statistics, true);

  // Should not have been called after deregister.
  EXPECT_EQ(1u, callback.num_calls_);
}

class RtpTestCallback : public StreamDataCountersCallback {
 public:
  RtpTestCallback()
      : StreamDataCountersCallback(), num_calls_(0), ssrc_(0), stats_() {}
  virtual ~RtpTestCallback() {}

  virtual void DataCountersUpdated(const StreamDataCounters& counters,
                                   uint32_t ssrc) {
    ssrc_ = ssrc;
    stats_ = counters;
    ++num_calls_;
  }

  void MatchPacketCounter(const RtpPacketCounter& expected,
                          const RtpPacketCounter& actual) {
    EXPECT_EQ(expected.payload_bytes, actual.payload_bytes);
    EXPECT_EQ(expected.header_bytes, actual.header_bytes);
    EXPECT_EQ(expected.padding_bytes, actual.padding_bytes);
    EXPECT_EQ(expected.packets, actual.packets);
  }

  void Matches(uint32_t num_calls,
               uint32_t ssrc,
               const StreamDataCounters& expected) {
    EXPECT_EQ(num_calls, num_calls_);
    EXPECT_EQ(ssrc, ssrc_);
    MatchPacketCounter(expected.transmitted, stats_.transmitted);
    MatchPacketCounter(expected.retransmitted, stats_.retransmitted);
    MatchPacketCounter(expected.fec, stats_.fec);
  }

  uint32_t num_calls_;
  uint32_t ssrc_;
  StreamDataCounters stats_;
};

TEST_F(ReceiveStatisticsTest, RtpCallbacks) {
  RtpTestCallback callback;
  receive_statistics_->RegisterRtpStatisticsCallback(&callback);

  const size_t kPaddingLength = 9;

  // One packet with payload size kPacketSize1, and non-minimal header size.
  std::unique_ptr<RtpPacketReceived> packet =
      CreatePacketWithPayloadAndExtraHeaders(kSsrc1, kPacketSize1);

  receive_statistics_->OnRtpPacket(*packet);
  StreamDataCounters expected;
  expected.transmitted.payload_bytes = kPacketSize1;
  expected.transmitted.header_bytes = packet->headers_size();
  expected.transmitted.padding_bytes = 0;
  expected.transmitted.packets = 1;
  expected.retransmitted.payload_bytes = 0;
  expected.retransmitted.header_bytes = 0;
  expected.retransmitted.padding_bytes = 0;
  expected.retransmitted.packets = 0;
  expected.fec.packets = 0;
  callback.Matches(1, kSsrc1, expected);

  IncrementSequenceNumber(packet.get());
  clock_.AdvanceTimeMilliseconds(5);

  // Another packet of size kPacketSize1 with 9 bytes padding.
  Random r(/*seed=*/1);
  packet->SetPadding(kPaddingLength, &r);
  receive_statistics_->OnRtpPacket(*packet);
  expected.transmitted.payload_bytes = kPacketSize1 * 2;
  expected.transmitted.header_bytes = packet->headers_size() * 2;
  expected.transmitted.padding_bytes = kPaddingLength;
  expected.transmitted.packets = 2;
  callback.Matches(2, kSsrc1, expected);

  clock_.AdvanceTimeMilliseconds(5);
  // Retransmit last packet.
  receive_statistics_->OnRtpPacket(*packet);
  expected.transmitted.payload_bytes = kPacketSize1 * 3;
  expected.transmitted.header_bytes = packet->headers_size() * 3;
  expected.transmitted.padding_bytes = kPaddingLength * 2;
  expected.transmitted.packets = 3;
  expected.retransmitted.payload_bytes = kPacketSize1;
  expected.retransmitted.header_bytes = packet->headers_size();
  expected.retransmitted.padding_bytes = kPaddingLength;
  expected.retransmitted.packets = 1;
  callback.Matches(3, kSsrc1, expected);

  packet->SetPadding(0, &r);
  IncrementSequenceNumber(packet.get());
  clock_.AdvanceTimeMilliseconds(5);
  // One FEC packet.
  // TODO(nisse): Refactor to also accept an RtpPacketReceived.
  receive_statistics_->OnRtpPacket(*packet);
  RTPHeader header;
  packet->GetHeader(&header);
  receive_statistics_->FecPacketReceived(header,
                                         kPacketSize1 + packet->headers_size());
  expected.transmitted.payload_bytes = kPacketSize1 * 4;
  expected.transmitted.header_bytes = packet->headers_size() * 4;
  expected.transmitted.packets = 4;
  expected.fec.payload_bytes = kPacketSize1;
  expected.fec.header_bytes = packet->headers_size();
  expected.fec.packets = 1;
  callback.Matches(5, kSsrc1, expected);

  receive_statistics_->RegisterRtpStatisticsCallback(NULL);

  // New stats, but callback should not be called.
  IncrementSequenceNumber(packet.get());
  clock_.AdvanceTimeMilliseconds(5);
  receive_statistics_->OnRtpPacket(*packet);
  callback.Matches(5, kSsrc1, expected);
}

TEST_F(ReceiveStatisticsTest, RtpCallbacksFecFirst) {
  RtpTestCallback callback;
  receive_statistics_->RegisterRtpStatisticsCallback(&callback);

  // One packet with payload size kPacketSize1, and non-minimal header size.
  std::unique_ptr<RtpPacketReceived> packet =
      CreatePacketWithPayloadAndExtraHeaders(kSsrc1, kPacketSize1);

  RTPHeader header;
  packet->GetHeader(&header);
  // If first packet is FEC, ignore it.
  receive_statistics_->FecPacketReceived(header,
                                         kPacketSize1 + packet->headers_size());
  EXPECT_EQ(0u, callback.num_calls_);

  receive_statistics_->OnRtpPacket(*packet);
  StreamDataCounters expected;
  expected.transmitted.payload_bytes = kPacketSize1;
  expected.transmitted.header_bytes = packet->headers_size();
  expected.transmitted.padding_bytes = 0;
  expected.transmitted.packets = 1;
  expected.fec.packets = 0;
  callback.Matches(1, kSsrc1, expected);

  receive_statistics_->FecPacketReceived(header,
                                         kPacketSize1 + packet->headers_size());
  expected.fec.payload_bytes = kPacketSize1;
  expected.fec.header_bytes = packet->headers_size();
  expected.fec.packets = 1;
  callback.Matches(2, kSsrc1, expected);
}

}  // namespace webrtc
