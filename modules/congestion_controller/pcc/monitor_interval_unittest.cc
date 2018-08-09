/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/pcc/monitor_interval.h"
#include "test/gtest.h"

namespace webrtc {
namespace pcc {
namespace test {
namespace {
const DataRate kTargetSendingRate = DataRate::kbps(300);
const Timestamp kStartTime = Timestamp::us(0);
const TimeDelta kIntervalDuration = TimeDelta::ms(100);
const TimeDelta kDefaultRtt = TimeDelta::ms(100);
const DataSize kDefaultPacketSize = DataSize::bytes(100);

std::vector<PacketResult> CreatePacketResults(
    const std::vector<Timestamp>& packets_send_times,
    const std::vector<Timestamp>& packets_received_times = {},
    const std::vector<DataSize>& packets_sizes = {}) {
  std::vector<PacketResult> packet_results;
  for (size_t i = 0; i < packets_send_times.size(); ++i) {
    SentPacket sent_packet;
    sent_packet.send_time = packets_send_times[i];
    if (packets_sizes.empty()) {
      sent_packet.size = kDefaultPacketSize;
    } else {
      sent_packet.size = packets_sizes[i];
    }
    PacketResult packet_result;
    packet_result.sent_packet = sent_packet;
    if (packets_received_times.empty()) {
      packet_result.receive_time = packets_send_times[i] + kDefaultRtt;
    } else {
      packet_result.receive_time = packets_received_times[i];
    }
    packet_results.push_back(packet_result);
  }
  return packet_results;
}

}  // namespace

TEST(PccMonitorIntervalTest, InitialValuesAreEqualToOnesSetInConstructor) {
  PccMonitorInterval interval{kTargetSendingRate, kStartTime,
                              kIntervalDuration};
  EXPECT_EQ(interval.IsFeedbackCollectionDone(), false);
  EXPECT_EQ(interval.GetEndTime(), kStartTime + kIntervalDuration);
  EXPECT_EQ(interval.GetTargetSendingRate(), kTargetSendingRate);
}

TEST(PccMonitorIntervalTest, IndicatesDoneWhenFeedbackReceivedAfterInterval) {
  PccMonitorInterval interval{kTargetSendingRate, kStartTime,
                              kIntervalDuration};
  interval.OnPacketsFeedback(CreatePacketResults({kStartTime}));
  EXPECT_EQ(interval.IsFeedbackCollectionDone(), false);
  interval.OnPacketsFeedback(
      CreatePacketResults({kStartTime, kStartTime + kIntervalDuration}));
  EXPECT_EQ(interval.IsFeedbackCollectionDone(), false);
  interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kIntervalDuration, kStartTime + 2 * kIntervalDuration}));
  EXPECT_EQ(interval.IsFeedbackCollectionDone(), true);
}

TEST(PccMonitorIntervalTest, LossRateIsOneThirdIfLostOnePacketOutOfThree) {
  PccMonitorInterval interval{kTargetSendingRate, kStartTime,
                              kIntervalDuration};
  std::vector<Timestamp> start_times = {
      kStartTime, kStartTime + 0.1 * kIntervalDuration,
      kStartTime + 0.5 * kIntervalDuration, kStartTime + kIntervalDuration,
      kStartTime + 2 * kIntervalDuration};
  std::vector<Timestamp> end_times = {
      kStartTime + 2 * kIntervalDuration, kStartTime + 2 * kIntervalDuration,
      Timestamp::Infinity(), kStartTime + 2 * kIntervalDuration,
      kStartTime + 4 * kIntervalDuration};
  std::vector<DataSize> packet_sizes = {
      kDefaultPacketSize, 2 * kDefaultPacketSize, 3 * kDefaultPacketSize,
      4 * kDefaultPacketSize, 5 * kDefaultPacketSize};
  std::vector<PacketResult> packet_results =
      CreatePacketResults(start_times, end_times, packet_sizes);
  interval.OnPacketsFeedback(packet_results);
  EXPECT_EQ(interval.IsFeedbackCollectionDone(), true);

  EXPECT_DOUBLE_EQ(interval.GetLossRate(), 1. / 3);
}

}  // namespace test
}  // namespace pcc
}  // namespace webrtc
