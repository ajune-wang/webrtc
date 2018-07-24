/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "modules/congestion_controller/pcc/utility_function.h"
#include "test/gtest.h"

namespace webrtc {
namespace pcc {
namespace test {
namespace {
constexpr double kRttGradientCoefficient = 900;
constexpr double kLossCoefficient = 11.35;
constexpr double kThroughputPower = 0.9;
constexpr double kThroughputCoefficient = 1;
constexpr double kRttGradientThreshold = 0.01;

const Timestamp kStartTime = Timestamp::us(0);
const TimeDelta kPacketsDelta = TimeDelta::ms(1);
const TimeDelta kIntervalDuration = TimeDelta::ms(100);
const DataRate kSendingBitrate = DataRate::bps(1000);

const DataSize kDefaultDataSize = DataSize::bytes(100);
const TimeDelta kDefaultRtt = TimeDelta::ms(100);

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

TEST(PccVivaceUtilityFunctionTest, AllZeroCoefficients) {
  VivaceUtilityFunction utility_function(0, 0, kThroughputCoefficient,
                                         kThroughputPower, 0);
  MonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                   kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + 2 * kPacketsDelta,
       kStartTime + 3 * kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kPacketsDelta + kDefaultRtt, Timestamp::Infinity(),
       kStartTime + kDefaultRtt + 3 * kPacketsDelta, Timestamp::Infinity()},
      {kDefaultDataSize, kDefaultDataSize, kDefaultDataSize,
       kDefaultDataSize}));
  EXPECT_EQ(utility_function.ComputeUtilityFunction(monitor_interval),
            std::pow(kSendingBitrate.kbps(), kThroughputCoefficient));
}

TEST(PccVivaceUtilityFunctionTest, ZeroRttGradient) {
  VivaceUtilityFunction utility_function(
      kRttGradientCoefficient, 0, kThroughputCoefficient, kThroughputPower, 0);
  MonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                   kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + 2 * kPacketsDelta,
       kStartTime + 3 * kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kDefaultRtt, Timestamp::Infinity(),
       kStartTime + kDefaultRtt + 2 * kPacketsDelta, Timestamp::Infinity()},
      {kDefaultDataSize, kDefaultDataSize, kDefaultDataSize,
       kDefaultDataSize}));
  // RTT gradient should be zero, because both received packets have the
  // same RTT.
  EXPECT_EQ(utility_function.ComputeUtilityFunction(monitor_interval),
            std::pow(kSendingBitrate.kbps(), kThroughputCoefficient));
}

TEST(PccVivaceUtilityFunctionTest, OnePointAvailableForRttGradient) {
  VivaceUtilityFunction utility_function(
      kRttGradientCoefficient, 0, kThroughputCoefficient, kThroughputPower, 0);
  MonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                   kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kDefaultRtt, kStartTime + 3 * kIntervalDuration}, {}));
  // Only one received packet belongs to the monitor_interval, RTT gradient
  // should be zero in this case.
  EXPECT_EQ(utility_function.ComputeUtilityFunction(monitor_interval),
            std::pow(kSendingBitrate.kbps(), kThroughputCoefficient));
}

TEST(PccVivaceUtilityFunctionTest, RttGradientIsOne) {
  VivaceUtilityFunction utility_function(
      kRttGradientCoefficient, 0, kThroughputCoefficient, kThroughputPower, 0);
  MonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                   kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + 2 * kPacketsDelta,
       kStartTime + 3 * kPacketsDelta, kStartTime + 3 * kIntervalDuration},
      {kStartTime + kDefaultRtt, Timestamp::Infinity(),
       kStartTime + 4 * kPacketsDelta + kDefaultRtt,
       kStartTime + 3 * kIntervalDuration},
      {}));
  EXPECT_EQ(utility_function.ComputeUtilityFunction(monitor_interval),
            std::pow(kSendingBitrate.kbps(), kThroughputCoefficient) -
                kRttGradientCoefficient * kSendingBitrate.kbps());
}

TEST(PccVivaceUtilityFunctionTest, RttGradientIsMinusOne) {
  VivaceUtilityFunction utility_function(
      kRttGradientCoefficient, 0, kThroughputCoefficient, kThroughputPower, 0);
  MonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                   kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + 2 * kPacketsDelta,
       kStartTime + 5 * kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kDefaultRtt, Timestamp::Infinity(),
       kStartTime + kDefaultRtt, kStartTime + 3 * kIntervalDuration},
      {}));
  EXPECT_EQ(utility_function.ComputeUtilityFunction(monitor_interval),
            std::pow(kSendingBitrate.kbps(), kThroughputCoefficient) +
                kRttGradientCoefficient * kSendingBitrate.kbps());
}

TEST(PccVivaceUtilityFunctionTest, NonZeroLossCoefficient) {
  VivaceUtilityFunction utility_function(
      0, kLossCoefficient, kThroughputCoefficient, kThroughputPower, 0);
  MonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                   kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + 2 * kPacketsDelta,
       kStartTime + 5 * kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kDefaultRtt, Timestamp::Infinity(),
       kStartTime + kDefaultRtt, kStartTime + 3 * kIntervalDuration},
      {}));
  // The second packet was lost.
  EXPECT_EQ(utility_function.ComputeUtilityFunction(monitor_interval),
            std::pow(kSendingBitrate.kbps(), kThroughputCoefficient) -
                kLossCoefficient * kSendingBitrate.kbps() *
                    monitor_interval.GetLossRate());
}

TEST(PccVivaceUtilityFunctionTest, RttGradientThreshold) {
  VivaceUtilityFunction utility_function(
      kRttGradientCoefficient, kLossCoefficient, kThroughputCoefficient,
      kThroughputPower, kRttGradientThreshold);
  MonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                   kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + kPacketsDelta,
       kStartTime + 102 * kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kDefaultRtt, Timestamp::Infinity(),
       kStartTime + kDefaultRtt + kPacketsDelta,
       kStartTime + 3 * kIntervalDuration},
      {}));
  // RTT gradient is less than 0.01 hence should be treated ad zero.
  EXPECT_EQ(utility_function.ComputeUtilityFunction(monitor_interval),
            std::pow(kSendingBitrate.kbps(), kThroughputCoefficient) -
                kLossCoefficient * kSendingBitrate.kbps() *
                    monitor_interval.GetLossRate());
}

TEST(PccVivaceUtilityFunctionTest, AllPacketsSentAtTheSameTime) {
  VivaceUtilityFunction utility_function(
      kRttGradientCoefficient, kLossCoefficient, kThroughputCoefficient,
      kThroughputPower, kRttGradientThreshold);
  MonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                   kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + kPacketsDelta,
       kStartTime + kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kDefaultRtt, Timestamp::Infinity(),
       kStartTime + kDefaultRtt + kPacketsDelta,
       kStartTime + 3 * kIntervalDuration},
      {}));
  // If all packets were sent at the same time, then RTT gradient should be
  // zero.
  EXPECT_EQ(utility_function.ComputeUtilityFunction(monitor_interval),
            std::pow(kSendingBitrate.kbps(), kThroughputCoefficient) -
                kLossCoefficient * kSendingBitrate.kbps() *
                    monitor_interval.GetLossRate());
}

}  // namespace test
}  // namespace pcc
}  // namespace webrtc
