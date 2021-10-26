/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/robust_throughput_estimator.h"

#include <algorithm>

#include "api/transport/field_trial_based_config.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
std::vector<PacketResult> CreateFeedbackVector(size_t number_of_packets,
                                               DataSize packet_size,
                                               TimeDelta send_increment,
                                               TimeDelta recv_increment,
                                               Timestamp* send_clock,
                                               Timestamp* recv_clock,
                                               uint16_t* sequence_number) {
  std::vector<PacketResult> packet_feedback_vector(number_of_packets);
  for (size_t i = 0; i < number_of_packets; i++) {
    packet_feedback_vector[i].receive_time = *recv_clock;
    packet_feedback_vector[i].sent_packet.send_time = *send_clock;
    packet_feedback_vector[i].sent_packet.sequence_number = *sequence_number;
    packet_feedback_vector[i].sent_packet.size = packet_size;
    *send_clock += send_increment;
    *recv_clock += recv_increment;
    *sequence_number += 1;
  }
  return packet_feedback_vector;
}
}  // anonymous namespace

TEST(RobustThroughputEstimatorTest, SteadyRate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:10,"
      "window_duration:100ms/");
  FieldTrialBasedConfig field_trial_config;
  RobustThroughputEstimatorSettings settings(&field_trial_config);
  RobustThroughputEstimator throughput_estimator(settings);
  DataSize packet_size(DataSize::Bytes(1000));
  Timestamp send_clock(Timestamp::Millis(100000));
  Timestamp recv_clock(Timestamp::Millis(10000));
  TimeDelta send_increment(TimeDelta::Millis(10));
  TimeDelta recv_increment(TimeDelta::Millis(10));
  uint16_t sequence_number = 100;
  std::vector<PacketResult> packet_feedback =
      CreateFeedbackVector(9, packet_size, send_increment, recv_increment,
                           &send_clock, &recv_clock, &sequence_number);
  throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
  EXPECT_FALSE(throughput_estimator.bitrate().has_value());

  packet_feedback =
      CreateFeedbackVector(11, packet_size, send_increment, recv_increment,
                           &send_clock, &recv_clock, &sequence_number);
  throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator.bitrate();
  EXPECT_TRUE(throughput.has_value());
  double expected_bytes_per_sec = 100 * 1000.0;
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              expected_bytes_per_sec,
              0.05 * expected_bytes_per_sec);  // Allow 5% error
}

TEST(RobustThroughputEstimatorTest, DelaySpike) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:10,"
      "window_duration:100ms/");
  FieldTrialBasedConfig field_trial_config;
  RobustThroughputEstimatorSettings settings(&field_trial_config);
  RobustThroughputEstimator throughput_estimator(settings);
  DataSize packet_size(DataSize::Bytes(1000));
  Timestamp send_clock(Timestamp::Millis(100000));
  Timestamp recv_clock(Timestamp::Millis(10000));
  TimeDelta send_increment(TimeDelta::Millis(10));
  TimeDelta recv_increment(TimeDelta::Millis(10));
  uint16_t sequence_number = 100;
  std::vector<PacketResult> packet_feedback =
      CreateFeedbackVector(20, packet_size, send_increment, recv_increment,
                           &send_clock, &recv_clock, &sequence_number);
  throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator.bitrate();
  EXPECT_TRUE(throughput.has_value());
  double expected_bytes_per_sec = 100 * 1000.0;
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              expected_bytes_per_sec,
              0.05 * expected_bytes_per_sec);  // Allow 5% error

  // Delay spike
  recv_clock += TimeDelta::Millis(40);

  // Faster delivery after the gap
  recv_increment = TimeDelta::Millis(2);
  packet_feedback =
      CreateFeedbackVector(5, packet_size, send_increment, recv_increment,
                           &send_clock, &recv_clock, &sequence_number);
  throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
  throughput = throughput_estimator.bitrate();
  EXPECT_TRUE(throughput.has_value());
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              expected_bytes_per_sec,
              0.05 * expected_bytes_per_sec);  // Allow 5% error

  // Delivery at normal rate. This will be capped by the send rate.
  recv_increment = TimeDelta::Millis(10);
  packet_feedback =
      CreateFeedbackVector(5, packet_size, send_increment, recv_increment,
                           &send_clock, &recv_clock, &sequence_number);
  throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
  throughput = throughput_estimator.bitrate();
  EXPECT_TRUE(throughput.has_value());
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              expected_bytes_per_sec,
              0.05 * expected_bytes_per_sec);  // Allow 5% error
}

TEST(RobustThroughputEstimatorTest, CappedByReceiveRate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:10,"
      "window_duration:100ms/");
  FieldTrialBasedConfig field_trial_config;
  RobustThroughputEstimatorSettings settings(&field_trial_config);
  RobustThroughputEstimator throughput_estimator(settings);
  DataSize packet_size(DataSize::Bytes(1000));
  Timestamp send_clock(Timestamp::Millis(100000));
  Timestamp recv_clock(Timestamp::Millis(10000));
  TimeDelta send_increment(TimeDelta::Millis(10));
  TimeDelta recv_increment(TimeDelta::Millis(40));
  uint16_t sequence_number = 100;
  std::vector<PacketResult> packet_feedback =
      CreateFeedbackVector(20, packet_size, send_increment, recv_increment,
                           &send_clock, &recv_clock, &sequence_number);
  throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator.bitrate();
  EXPECT_TRUE(throughput.has_value());
  double expected_bytes_per_sec = 25 * 1000.0;
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              expected_bytes_per_sec,
              0.05 * expected_bytes_per_sec);  // Allow 5% error
}

TEST(RobustThroughputEstimatorTest, CappedBySendRate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:10,"
      "window_duration:100ms/");
  FieldTrialBasedConfig field_trial_config;
  RobustThroughputEstimatorSettings settings(&field_trial_config);
  RobustThroughputEstimator throughput_estimator(settings);
  DataSize packet_size(DataSize::Bytes(1000));
  Timestamp send_clock(Timestamp::Millis(100000));
  Timestamp recv_clock(Timestamp::Millis(10000));
  TimeDelta send_increment(TimeDelta::Millis(20));
  TimeDelta recv_increment(TimeDelta::Millis(10));
  uint16_t sequence_number = 100;
  std::vector<PacketResult> packet_feedback =
      CreateFeedbackVector(20, packet_size, send_increment, recv_increment,
                           &send_clock, &recv_clock, &sequence_number);
  throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator.bitrate();
  EXPECT_TRUE(throughput.has_value());
  double expected_bytes_per_sec = 50 * 1000.0;
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              expected_bytes_per_sec,
              0.05 * expected_bytes_per_sec);  // Allow 5% error
}

TEST(RobustThroughputEstimatorTest, HighLoss) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:10,"
      "window_duration:100ms/");
  FieldTrialBasedConfig field_trial_config;
  RobustThroughputEstimatorSettings settings(&field_trial_config);
  RobustThroughputEstimator throughput_estimator(settings);
  size_t number_of_packets = 20;
  DataSize packet_size(DataSize::Bytes(1000));
  Timestamp send_clock(Timestamp::Millis(100000));
  Timestamp recv_clock(Timestamp::Millis(10000));
  TimeDelta send_increment(TimeDelta::Millis(10));
  TimeDelta recv_increment(TimeDelta::Millis(10));
  uint16_t sequence_number = 100;
  std::vector<PacketResult> packet_feedback(number_of_packets);
  for (size_t i = 0; i < number_of_packets; i++) {
    if (i % 2 == 1) {
      // Packet lost.
      packet_feedback[i].receive_time = Timestamp::PlusInfinity();
    } else {
      packet_feedback[i].receive_time = recv_clock;
    }
    packet_feedback[i].sent_packet.send_time = send_clock;
    packet_feedback[i].sent_packet.sequence_number = sequence_number;
    packet_feedback[i].sent_packet.size = packet_size;
    send_clock += send_increment;
    recv_clock += recv_increment;
    sequence_number += 1;
  }

  std::sort(packet_feedback.begin(), packet_feedback.end(),
            PacketResult::ReceiveTimeOrder());
  throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator.bitrate();
  EXPECT_TRUE(throughput.has_value());
  double expected_bytes_per_sec = 50 * 1000.0;
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              expected_bytes_per_sec,
              0.05 * expected_bytes_per_sec);  // Allow 5% error
}

TEST(RobustThroughputEstimatorTest, DeepReordering) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:10,"
      "window_duration:100ms/");
  FieldTrialBasedConfig field_trial_config;
  RobustThroughputEstimatorSettings settings(&field_trial_config);
  RobustThroughputEstimator throughput_estimator(settings);
  size_t number_of_packets = 10;
  DataSize packet_size(DataSize::Bytes(1000));
  Timestamp send_clock(Timestamp::Millis(100000));
  Timestamp recv_clock(Timestamp::Millis(10000));
  TimeDelta send_increment(TimeDelta::Millis(10));
  TimeDelta recv_increment(TimeDelta::Millis(10));
  uint16_t sequence_number = 100;
  std::vector<PacketResult> packet_feedback(number_of_packets);

  PacketResult delayed_packet;
  delayed_packet.sent_packet.size = packet_size;
  delayed_packet.sent_packet.send_time = send_clock;
  delayed_packet.sent_packet.sequence_number = sequence_number;
  send_clock += send_increment;
  recv_clock += recv_increment;
  sequence_number += 1;

  for (int i = 0; i < 4; i++) {
    std::vector<PacketResult> packet_feedback =
        CreateFeedbackVector(10, packet_size, send_increment, recv_increment,
                             &send_clock, &recv_clock, &sequence_number);
    throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
    auto throughput = throughput_estimator.bitrate();
    EXPECT_TRUE(throughput.has_value());
    double expected_bytes_per_sec = 100 * 1000.0;
    EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
                expected_bytes_per_sec,
                0.05 * expected_bytes_per_sec);  // Allow 5% error
  }

  // Delayed packet arrives.
  delayed_packet.receive_time = recv_clock;
  throughput_estimator.IncomingPacketFeedbackVector({delayed_packet});
  auto throughput = throughput_estimator.bitrate();
  EXPECT_TRUE(throughput.has_value());
  double expected_bytes_per_sec = 100 * 1000.0;
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              expected_bytes_per_sec,
              0.05 * expected_bytes_per_sec);  // Allow 5% error

  for (int i = 0; i < 4; i++) {
    std::vector<PacketResult> packet_feedback =
        CreateFeedbackVector(5, packet_size, send_increment, recv_increment,
                             &send_clock, &recv_clock, &sequence_number);
    throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
    auto throughput = throughput_estimator.bitrate();
    EXPECT_TRUE(throughput.has_value());
    double expected_bytes_per_sec = 100 * 1000.0;
    EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
                expected_bytes_per_sec,
                0.05 * expected_bytes_per_sec);  // Allow 5% error
  }
}

TEST(RobustThroughputEstimatorTest, StreamPausedAndResumed) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:10,"
      "window_duration:100ms/");
  FieldTrialBasedConfig field_trial_config;
  RobustThroughputEstimatorSettings settings(&field_trial_config);
  RobustThroughputEstimator throughput_estimator(settings);
  DataSize packet_size(DataSize::Bytes(1000));
  Timestamp send_clock(Timestamp::Millis(100000));
  Timestamp recv_clock(Timestamp::Millis(10000));
  TimeDelta send_increment(TimeDelta::Millis(10));
  TimeDelta recv_increment(TimeDelta::Millis(10));
  uint16_t sequence_number = 100;
  std::vector<PacketResult> packet_feedback =
      CreateFeedbackVector(20, packet_size, send_increment, recv_increment,
                           &send_clock, &recv_clock, &sequence_number);
  throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator.bitrate();
  EXPECT_TRUE(throughput.has_value());
  double expected_bytes_per_sec = 100 * 1000.0;
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              expected_bytes_per_sec,
              0.05 * expected_bytes_per_sec);  // Allow 5% error

  // No packets sent or feedback received for 60s.
  send_clock += TimeDelta::Seconds(60);
  recv_clock += TimeDelta::Seconds(60);

  // Resume sending packets as the same rate as before. The estimate
  // will initially be invalid, due to lack of recent data.
  packet_feedback =
      CreateFeedbackVector(5, packet_size, send_increment, recv_increment,
                           &send_clock, &recv_clock, &sequence_number);
  throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
  throughput = throughput_estimator.bitrate();
  EXPECT_FALSE(throughput.has_value());

  // But be back to the normal level once we have enough data.
  for (int i = 0; i < 4; ++i) {
    packet_feedback =
        CreateFeedbackVector(5, packet_size, send_increment, recv_increment,
                             &send_clock, &recv_clock, &sequence_number);
    throughput_estimator.IncomingPacketFeedbackVector(packet_feedback);
    throughput = throughput_estimator.bitrate();
    EXPECT_TRUE(throughput.has_value());
    EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
                expected_bytes_per_sec,
                0.05 * expected_bytes_per_sec);  // Allow 5% error
  }
}

}  // namespace webrtc*/
