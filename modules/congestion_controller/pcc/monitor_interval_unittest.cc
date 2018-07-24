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
const DataRate kTargetSendingRate = DataRate::bps(100);
// const double kEpsilon = 0.05;
const Timestamp kStartTime = Timestamp::us(0);
const TimeDelta kIntervalDuration = TimeDelta::us(10);
const TimeDelta kDefaultRtt = TimeDelta::us(10);
const DataSize kDefaultDataSize = DataSize::bytes(100);
const TimeDelta kMiTimeout = kIntervalDuration * 4;

std::vector<PacketResult> CreatePacketResults(
    const std::vector<Timestamp>& packets_send_times,
    const std::vector<Timestamp>& packets_received_times = {},
    const std::vector<DataSize>& packets_sizes = {}) {
  std::vector<PacketResult> packet_results;
  PacketResult packet_result;
  SentPacket sent_packet;
  for (size_t i = 0; i < packets_send_times.size(); ++i) {
    sent_packet.send_time = packets_send_times[i];
    if (packets_sizes.empty()) {
      sent_packet.size = kDefaultDataSize;
    } else {
      sent_packet.size = packets_sizes[i];
    }
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

TEST(PccMonitorIntervalTest, InitialValues) {
  MonitorInterval interval{kTargetSendingRate, kStartTime, kIntervalDuration};
  EXPECT_EQ(interval.IsFeedbackCollectionDone(), false);
  EXPECT_EQ(interval.GetEndTime(), kStartTime + kIntervalDuration);
  //  EXPECT_EQ(interval.interval_duration, kIntervalDuration);
  EXPECT_EQ(interval.GetTargetSendingRate(), kTargetSendingRate);
}

TEST(PccMonitorIntervalTest, CollectingFeedback) {
  MonitorInterval interval{kTargetSendingRate, kStartTime, kIntervalDuration};
  interval.OnPacketsFeedback(CreatePacketResults({kStartTime}));
  EXPECT_EQ(interval.IsFeedbackCollectionDone(), false);
  interval.OnPacketsFeedback(
      CreatePacketResults({kStartTime, kStartTime + kIntervalDuration}));
  EXPECT_EQ(interval.IsFeedbackCollectionDone(), false);
  interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kIntervalDuration, kStartTime + 2 * kIntervalDuration}));
  EXPECT_EQ(interval.IsFeedbackCollectionDone(), true);
}

TEST(PccMonitorIntervalTest, ReceivedPacketsInfo) {
  MonitorInterval interval{kTargetSendingRate, kStartTime, kIntervalDuration};
  std::vector<Timestamp> start_times = {
      kStartTime, kStartTime + 0.1 * kIntervalDuration,
      kStartTime + 0.5 * kIntervalDuration, kStartTime + kIntervalDuration,
      kStartTime + 2 * kIntervalDuration};
  std::vector<Timestamp> end_times = {
      kStartTime + 2 * kIntervalDuration, kStartTime + 2 * kIntervalDuration,
      Timestamp::Infinity(), kStartTime + 2 * kIntervalDuration,
      kStartTime + 4 * kIntervalDuration};
  std::vector<DataSize> packet_sizes = {
      kDefaultDataSize, 2 * kDefaultDataSize, 3 * kDefaultDataSize,
      4 * kDefaultDataSize, 5 * kDefaultDataSize};
  std::vector<PacketResult> packet_results =
      CreatePacketResults(start_times, end_times, packet_sizes);
  interval.OnPacketsFeedback(packet_results);
  EXPECT_EQ(interval.IsFeedbackCollectionDone(), true);
  EXPECT_EQ(interval.GetReceivedPacketsRtt().size(), static_cast<size_t>(2));
  EXPECT_EQ(interval.GetReceivedPacketsRtt()[0], end_times[1] - start_times[1]);
  EXPECT_EQ(interval.GetReceivedPacketsRtt()[1], end_times[3] - start_times[3]);

  EXPECT_EQ(interval.GetReceivedPacketsSentTime().size(),
            static_cast<size_t>(2));
  EXPECT_EQ(interval.GetReceivedPacketsSentTime()[0], start_times[1]);
  EXPECT_EQ(interval.GetReceivedPacketsSentTime()[1], start_times[3]);

  EXPECT_EQ(interval.GetLostPacketsSentTime().size(), static_cast<size_t>(1));
  EXPECT_EQ(interval.GetLostPacketsSentTime()[0], start_times[2]);

  EXPECT_EQ(interval.GetLossRate(), 1. / 3);
}

}  // namespace test
}  // namespace pcc
}  // namespace webrtc
