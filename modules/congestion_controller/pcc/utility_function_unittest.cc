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
constexpr double kDelayGradientCoefficient = 900;
constexpr double kLossCoefficient = 11.35;
constexpr double kThroughputPower = 0.9;
constexpr double kThroughputCoefficient = 1;
constexpr double kDelayGradientThreshold = 0.01;
constexpr double kDelayGradientNegativeBound = 10;

const Timestamp kStartTime = Timestamp::us(0);
const TimeDelta kPacketsDelta = TimeDelta::ms(1);
const TimeDelta kIntervalDuration = TimeDelta::ms(100);
const DataRate kSendingBitrate = DataRate::bps(1000);

const DataSize kDefaultDataSize = DataSize::bytes(100);
const TimeDelta kDefaultDelay = TimeDelta::ms(100);

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
      packet_result.receive_time = packets_send_times[i] + kDefaultDelay;
    } else {
      packet_result.receive_time = packets_received_times[i];
    }
    packet_results.push_back(packet_result);
  }
  return packet_results;
}

}  // namespace

TEST(PccVivaceUtilityFunctionTest,
     UtilityIsThroughputTermIfAllRestCoefficientsAreZero) {
  VivaceUtilityFunction utility_function(0, 0, kThroughputCoefficient,
                                         kThroughputPower, 0,
                                         kDelayGradientNegativeBound);
  PccMonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                      kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + 2 * kPacketsDelta,
       kStartTime + 3 * kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kPacketsDelta + kDefaultDelay, Timestamp::Infinity(),
       kStartTime + kDefaultDelay + 3 * kPacketsDelta, Timestamp::Infinity()},
      {kDefaultDataSize, kDefaultDataSize, kDefaultDataSize,
       kDefaultDataSize}));
  EXPECT_DOUBLE_EQ(utility_function.Compute(monitor_interval),
                   kThroughputCoefficient *
                       std::pow(kSendingBitrate.bps(), kThroughputPower));
}

TEST(PccVivaceUtilityFunctionTest, DelayGradientIsZeroIfNoChangeInPacketDelay) {
  VivaceUtilityFunction utility_function(
      kDelayGradientCoefficient, 0, kThroughputCoefficient, kThroughputPower, 0,
      kDelayGradientNegativeBound);
  PccMonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                      kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + 2 * kPacketsDelta,
       kStartTime + 3 * kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kDefaultDelay, Timestamp::Infinity(),
       kStartTime + kDefaultDelay + 2 * kPacketsDelta, Timestamp::Infinity()},
      {kDefaultDataSize, kDefaultDataSize, kDefaultDataSize,
       kDefaultDataSize}));
  // Delay gradient should be zero, because both received packets have the
  // same one way delay.
  EXPECT_DOUBLE_EQ(utility_function.Compute(monitor_interval),
                   kThroughputCoefficient *
                       std::pow(kSendingBitrate.bps(), kThroughputPower));
}

TEST(PccVivaceUtilityFunctionTest,
     DelayGradientIsZeroWhenOnePacketSentInMonitorInterval) {
  VivaceUtilityFunction utility_function(
      kDelayGradientCoefficient, 0, kThroughputCoefficient, kThroughputPower, 0,
      kDelayGradientNegativeBound);
  PccMonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                      kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kDefaultDelay, kStartTime + 3 * kIntervalDuration}, {}));
  // Only one received packet belongs to the monitor_interval, delay gradient
  // should be zero in this case.
  EXPECT_DOUBLE_EQ(utility_function.Compute(monitor_interval),
                   kThroughputCoefficient *
                       std::pow(kSendingBitrate.bps(), kThroughputPower));
}

TEST(PccVivaceUtilityFunctionTest, DelayGradientIsOne) {
  VivaceUtilityFunction utility_function(
      kDelayGradientCoefficient, 0, kThroughputCoefficient, kThroughputPower, 0,
      kDelayGradientNegativeBound);
  PccMonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                      kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + 2 * kPacketsDelta,
       kStartTime + 3 * kPacketsDelta, kStartTime + 3 * kIntervalDuration},
      {kStartTime + kDefaultDelay, Timestamp::Infinity(),
       kStartTime + 4 * kPacketsDelta + kDefaultDelay,
       kStartTime + 3 * kIntervalDuration},
      {}));
  EXPECT_DOUBLE_EQ(utility_function.Compute(monitor_interval),
                   kThroughputCoefficient *
                           std::pow(kSendingBitrate.bps(), kThroughputPower) -
                       kDelayGradientCoefficient * kSendingBitrate.bps());
}

TEST(PccVivaceUtilityFunctionTest, DelayGradientIsMinusOne) {
  VivaceUtilityFunction utility_function(
      kDelayGradientCoefficient, 0, kThroughputCoefficient, kThroughputPower, 0,
      kDelayGradientNegativeBound);
  PccMonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                      kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + 2 * kPacketsDelta,
       kStartTime + 5 * kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kDefaultDelay, Timestamp::Infinity(),
       kStartTime + kDefaultDelay, kStartTime + 3 * kIntervalDuration},
      {}));
  EXPECT_DOUBLE_EQ(utility_function.Compute(monitor_interval),
                   kThroughputCoefficient *
                           std::pow(kSendingBitrate.bps(), kThroughputPower) +
                       kDelayGradientCoefficient * kSendingBitrate.bps());
}

TEST(PccVivaceUtilityFunctionTest, LosTermIsNonZeroIfLossCoefficientIsNonZero) {
  VivaceUtilityFunction utility_function(
      0, kLossCoefficient, kThroughputCoefficient, kThroughputPower, 0,
      kDelayGradientNegativeBound);
  PccMonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                      kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + 2 * kPacketsDelta,
       kStartTime + 5 * kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kDefaultDelay, Timestamp::Infinity(),
       kStartTime + kDefaultDelay, kStartTime + 3 * kIntervalDuration},
      {}));
  // The second packet was lost.
  EXPECT_DOUBLE_EQ(utility_function.Compute(monitor_interval),
                   kThroughputCoefficient *
                           std::pow(kSendingBitrate.bps(), kThroughputPower) -
                       kLossCoefficient * kSendingBitrate.bps() *
                           monitor_interval.GetLossRate());
}

TEST(PccVivaceUtilityFunctionTest,
     DelayGradientIsZeroIfItSmallerWhenGradientThreshold) {
  VivaceUtilityFunction utility_function(
      kDelayGradientCoefficient, kLossCoefficient, kThroughputCoefficient,
      kThroughputPower, kDelayGradientThreshold, kDelayGradientNegativeBound);
  PccMonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                      kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + kPacketsDelta,
       kStartTime + 102 * kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kDefaultDelay, Timestamp::Infinity(),
       kStartTime + kDefaultDelay + kPacketsDelta,
       kStartTime + 3 * kIntervalDuration},
      {}));
  // Delay gradient is less than 0.01 hence should be treated as zero.
  EXPECT_DOUBLE_EQ(utility_function.Compute(monitor_interval),
                   kThroughputCoefficient *
                           std::pow(kSendingBitrate.bps(), kThroughputPower) -
                       kLossCoefficient * kSendingBitrate.bps() *
                           monitor_interval.GetLossRate());
}

TEST(PccVivaceUtilityFunctionTest,
     DelayGradientIsZeroWhenAllPacketsSentAtTheSameTime) {
  VivaceUtilityFunction utility_function(
      kDelayGradientCoefficient, kLossCoefficient, kThroughputCoefficient,
      kThroughputPower, kDelayGradientThreshold, kDelayGradientNegativeBound);
  PccMonitorInterval monitor_interval(kSendingBitrate, kStartTime,
                                      kIntervalDuration);
  monitor_interval.OnPacketsFeedback(CreatePacketResults(
      {kStartTime + kPacketsDelta, kStartTime + kPacketsDelta,
       kStartTime + kPacketsDelta, kStartTime + 2 * kIntervalDuration},
      {kStartTime + kDefaultDelay, Timestamp::Infinity(),
       kStartTime + kDefaultDelay + kPacketsDelta,
       kStartTime + 3 * kIntervalDuration},
      {}));
  // If all packets were sent at the same time, then delay gradient should be
  // zero.
  EXPECT_DOUBLE_EQ(utility_function.Compute(monitor_interval),
                   kThroughputCoefficient *
                           std::pow(kSendingBitrate.bps(), kThroughputPower) -
                       kLossCoefficient * kSendingBitrate.bps() *
                           monitor_interval.GetLossRate());
}

}  // namespace test
}  // namespace pcc
}  // namespace webrtc
