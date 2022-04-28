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

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>

#include "api/transport/field_trial_based_config.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {

class RobustThroughputEstimatorTest : public ::testing::Test {
 protected:
  RobustThroughputEstimatorTest() {}

  std::unique_ptr<RobustThroughputEstimator> CreateRobustThroughputEstimator()
      const {
    FieldTrialBasedConfig field_trial_config;
    RobustThroughputEstimatorSettings settings(&field_trial_config);
    return std::make_unique<RobustThroughputEstimator>(settings);
  }

  std::vector<PacketResult> CreateFeedbackVector(size_t number_of_packets,
                                                 DataSize packet_size,
                                                 DataRate send_rate,
                                                 DataRate recv_rate) {
    std::vector<PacketResult> packet_feedback_vector(number_of_packets);
    for (size_t i = 0; i < number_of_packets; i++) {
      packet_feedback_vector[i].sent_packet.send_time = send_clock_;
      packet_feedback_vector[i].sent_packet.sequence_number = sequence_number_;
      packet_feedback_vector[i].sent_packet.size = packet_size;
      send_clock_ += packet_size / send_rate;
      recv_clock_ += packet_size / recv_rate;
      sequence_number_ += 1;
      packet_feedback_vector[i].receive_time = recv_clock_;
    }
    return packet_feedback_vector;
  }

  DataSize default_packet_size_ = DataSize::Bytes(1000);
  Timestamp send_clock_ = Timestamp::Millis(100000);
  Timestamp recv_clock_ = Timestamp::Millis(10000);
  uint16_t sequence_number_ = 100;
};

TEST_F(RobustThroughputEstimatorTest, InitialEstimate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:20,max_window_packets:500,"
      "window_duration:1000ms,max_window_duration:5000ms,"
      "required_packets:10,unacked_weight:1.0/");
  std::unique_ptr<RobustThroughputEstimator> throughput_estimator =
      CreateRobustThroughputEstimator();
  DataRate send_rate(DataRate::BytesPerSec(100000));
  DataRate recv_rate(DataRate::BytesPerSec(100000));

  // No estimate until the estimator has enough data.
  std::vector<PacketResult> packet_feedback =
      CreateFeedbackVector(9, default_packet_size_, send_rate, recv_rate);
  throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
  EXPECT_FALSE(throughput_estimator->bitrate().has_value());

  // Estimate once `required_packets` packets have been received.
  packet_feedback =
      CreateFeedbackVector(1, default_packet_size_, send_rate, recv_rate);
  throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator->bitrate();
  ASSERT_TRUE(throughput.has_value());
  EXPECT_EQ(throughput.value(), send_rate);

  // Estimate remains stable when send and receive rates are stable.
  packet_feedback =
      CreateFeedbackVector(15, default_packet_size_, send_rate, recv_rate);
  throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
  throughput = throughput_estimator->bitrate();
  ASSERT_TRUE(throughput.has_value());
  EXPECT_EQ(throughput.value(), send_rate);
}

TEST_F(RobustThroughputEstimatorTest, EstimateAdapts) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:20,max_window_packets:500,"
      "window_duration:1000ms,max_window_duration:5000ms,"
      "required_packets:10,unacked_weight:1.0/");
  std::unique_ptr<RobustThroughputEstimator> throughput_estimator =
      CreateRobustThroughputEstimator();

  // 1 second, 800kbps, estimate is stable.
  DataRate send_rate(DataRate::BytesPerSec(100000));
  DataRate recv_rate(DataRate::BytesPerSec(100000));
  for (int i = 0; i < 10; ++i) {
    std::vector<PacketResult> packet_feedback =
        CreateFeedbackVector(10, default_packet_size_, send_rate, recv_rate);
    throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
    auto throughput = throughput_estimator->bitrate();
    ASSERT_TRUE(throughput.has_value());
    EXPECT_EQ(throughput.value(), send_rate);
  }

  // 1 second, 1600kbps, estimate increases
  send_rate = DataRate::BytesPerSec(200000);
  recv_rate = DataRate::BytesPerSec(200000);
  for (int i = 0; i < 20; ++i) {
    std::vector<PacketResult> packet_feedback =
        CreateFeedbackVector(10, default_packet_size_, send_rate, recv_rate);
    throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
    auto throughput = throughput_estimator->bitrate();
    ASSERT_TRUE(throughput.has_value());
    EXPECT_GE(throughput.value(), DataRate::BytesPerSec(100000));
    EXPECT_LE(throughput.value(), send_rate);
  }

  // 1 second, 1600kbps, estimate is stable
  for (int i = 0; i < 20; ++i) {
    std::vector<PacketResult> packet_feedback =
        CreateFeedbackVector(10, default_packet_size_, send_rate, recv_rate);
    throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
    auto throughput = throughput_estimator->bitrate();
    ASSERT_TRUE(throughput.has_value());
    EXPECT_EQ(throughput.value(), send_rate);
  }

  // 1 second, 400kbps, estimate decreases
  send_rate = DataRate::BytesPerSec(50000);
  recv_rate = DataRate::BytesPerSec(50000);
  for (int i = 0; i < 5; ++i) {
    std::vector<PacketResult> packet_feedback =
        CreateFeedbackVector(10, default_packet_size_, send_rate, recv_rate);
    throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
    auto throughput = throughput_estimator->bitrate();
    ASSERT_TRUE(throughput.has_value());
    EXPECT_LE(throughput.value(), DataRate::BytesPerSec(200000));
    EXPECT_GE(throughput.value(), send_rate);
  }

  // 1 second, 400kbps, estimate is stable
  send_rate = DataRate::BytesPerSec(50000);
  recv_rate = DataRate::BytesPerSec(50000);
  for (int i = 0; i < 5; ++i) {
    std::vector<PacketResult> packet_feedback =
        CreateFeedbackVector(10, default_packet_size_, send_rate, recv_rate);
    throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
    auto throughput = throughput_estimator->bitrate();
    ASSERT_TRUE(throughput.has_value());
    EXPECT_EQ(throughput.value(), send_rate);
  }
}

TEST_F(RobustThroughputEstimatorTest, CappedByReceiveRate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:20,max_window_packets:500,"
      "window_duration:1000ms,max_window_duration:5000ms,"
      "required_packets:10,unacked_weight:1.0/");
  std::unique_ptr<RobustThroughputEstimator> throughput_estimator =
      CreateRobustThroughputEstimator();
  DataRate send_rate(DataRate::BytesPerSec(100000));
  DataRate recv_rate(DataRate::BytesPerSec(25000));

  std::vector<PacketResult> packet_feedback =
      CreateFeedbackVector(20, default_packet_size_, send_rate, recv_rate);
  throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator->bitrate();
  ASSERT_TRUE(throughput.has_value());
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              recv_rate.bytes_per_sec<double>(),
              0.05 * recv_rate.bytes_per_sec<double>());  // Allow 5% error
}

TEST_F(RobustThroughputEstimatorTest, CappedBySendRate) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:20,max_window_packets:500,"
      "window_duration:1000ms,max_window_duration:5000ms,"
      "required_packets:10,unacked_weight:1.0/");
  std::unique_ptr<RobustThroughputEstimator> throughput_estimator =
      CreateRobustThroughputEstimator();
  DataRate send_rate(DataRate::BytesPerSec(50000));
  DataRate recv_rate(DataRate::BytesPerSec(100000));

  std::vector<PacketResult> packet_feedback =
      CreateFeedbackVector(20, default_packet_size_, send_rate, recv_rate);
  throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator->bitrate();
  ASSERT_TRUE(throughput.has_value());
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              send_rate.bytes_per_sec<double>(),
              0.05 * send_rate.bytes_per_sec<double>());  // Allow 5% error
}

TEST_F(RobustThroughputEstimatorTest, DelaySpike) {
  // N.B. This test uses a 500ms window to amplify the effect
  // of a delay spike.
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:20,max_window_packets:500,"
      "window_duration:500ms,max_window_duration:5000ms,"
      "required_packets:10,unacked_weight:1.0/");
  std::unique_ptr<RobustThroughputEstimator> throughput_estimator =
      CreateRobustThroughputEstimator();
  DataRate send_rate(DataRate::BytesPerSec(100000));
  DataRate recv_rate(DataRate::BytesPerSec(100000));

  std::vector<PacketResult> packet_feedback =
      CreateFeedbackVector(20, default_packet_size_, send_rate, recv_rate);
  throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator->bitrate();
  ASSERT_TRUE(throughput.has_value());
  EXPECT_EQ(throughput.value(), send_rate);

  // Delay spike. 25 packets sent, but none received.
  recv_clock_ += TimeDelta::Millis(250);

  // Deliver all of the packets during the next 50 ms. (During this time,
  // we'll have sent an additional 5 packets, so we need to receive 30
  // packets at 1000 bytes each in 50 ms, i.e. 600000 bytes per second).
  recv_rate = DataRate::BytesPerSec(600000);
  // Estimate should not drop.
  for (int i = 0; i < 30; ++i) {
    packet_feedback =
        CreateFeedbackVector(1, default_packet_size_, send_rate, recv_rate);
    throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
    throughput = throughput_estimator->bitrate();
    ASSERT_TRUE(throughput.has_value());
    EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
                send_rate.bytes_per_sec<double>(),
                0.05 * send_rate.bytes_per_sec<double>());  // Allow 5% error
  }

  // Delivery at normal rate. When the packets received before the gap
  // has left the estimator's window, the receive rate will be high, but the
  // estimate should be capped by the send rate.
  recv_rate = DataRate::BytesPerSec(100000);
  for (int i = 0; i < 20; ++i) {
    packet_feedback =
        CreateFeedbackVector(5, default_packet_size_, send_rate, recv_rate);
    throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
    throughput = throughput_estimator->bitrate();
    ASSERT_TRUE(throughput.has_value());
    EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
                send_rate.bytes_per_sec<double>(),
                0.05 * send_rate.bytes_per_sec<double>());  // Allow 5% error
  }
}

TEST_F(RobustThroughputEstimatorTest, HighLoss) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:20,max_window_packets:500,"
      "window_duration:1000ms,max_window_duration:5000ms,"
      "required_packets:10,unacked_weight:1.0/");
  std::unique_ptr<RobustThroughputEstimator> throughput_estimator =
      CreateRobustThroughputEstimator();
  DataRate send_rate(DataRate::BytesPerSec(100000));
  DataRate recv_rate(DataRate::BytesPerSec(100000));

  std::vector<PacketResult> packet_feedback =
      CreateFeedbackVector(20, default_packet_size_, send_rate, recv_rate);

  // 50% loss
  for (size_t i = 0; i < packet_feedback.size(); i++) {
    if (i % 2 == 1) {
      packet_feedback[i].receive_time = Timestamp::PlusInfinity();
    }
  }

  std::sort(packet_feedback.begin(), packet_feedback.end(),
            PacketResult::ReceiveTimeOrder());
  throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator->bitrate();
  ASSERT_TRUE(throughput.has_value());
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              send_rate.bytes_per_sec<double>() / 2,
              0.05 * send_rate.bytes_per_sec<double>() / 2);  // Allow 5% error
}

TEST_F(RobustThroughputEstimatorTest, ReorderedFeedback) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:20,max_window_packets:500,"
      "window_duration:1000ms,max_window_duration:5000ms,"
      "required_packets:10,unacked_weight:1.0/");
  std::unique_ptr<RobustThroughputEstimator> throughput_estimator =
      CreateRobustThroughputEstimator();
  DataRate send_rate(DataRate::BytesPerSec(100000));
  DataRate recv_rate(DataRate::BytesPerSec(100000));

  std::vector<PacketResult> packet_feedback =
      CreateFeedbackVector(20, default_packet_size_, send_rate, recv_rate);
  throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator->bitrate();
  ASSERT_TRUE(throughput.has_value());
  EXPECT_EQ(throughput.value(), send_rate);

  std::vector<PacketResult> delayed_feedback =
      CreateFeedbackVector(10, default_packet_size_, send_rate, recv_rate);
  packet_feedback =
      CreateFeedbackVector(10, default_packet_size_, send_rate, recv_rate);

  // Since we're missing some feedback, it's expected that the
  // estimate will drop.
  throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
  throughput = throughput_estimator->bitrate();
  ASSERT_TRUE(throughput.has_value());
  EXPECT_LT(throughput.value(), send_rate);

  // But it should completely recover as soon as we get the feedback.
  throughput_estimator->IncomingPacketFeedbackVector(delayed_feedback);
  throughput = throughput_estimator->bitrate();
  ASSERT_TRUE(throughput.has_value());
  EXPECT_EQ(throughput.value(), send_rate);

  // It should then remain stable (as if the feedbacks weren't reordered.)
  for (int i = 0; i < 10; ++i) {
    packet_feedback =
        CreateFeedbackVector(15, default_packet_size_, send_rate, recv_rate);
    throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
    throughput = throughput_estimator->bitrate();
    ASSERT_TRUE(throughput.has_value());
    EXPECT_EQ(throughput.value(), send_rate);
  }
}

TEST_F(RobustThroughputEstimatorTest, DeepReordering) {
  // N.B. This test uses a 500ms window to amplify the
  // effect of reordering.
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:20,max_window_packets:500,"
      "window_duration:500ms,max_window_duration:5000ms,"
      "required_packets:10,unacked_weight:1.0/");
  std::unique_ptr<RobustThroughputEstimator> throughput_estimator =
      CreateRobustThroughputEstimator();
  DataRate send_rate(DataRate::BytesPerSec(100000));
  DataRate recv_rate(DataRate::BytesPerSec(100000));

  std::vector<PacketResult> delayed_packets =
      CreateFeedbackVector(1, default_packet_size_, send_rate, recv_rate);

  for (int i = 0; i < 10; i++) {
    std::vector<PacketResult> packet_feedback =
        CreateFeedbackVector(10, default_packet_size_, send_rate, recv_rate);
    throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
    auto throughput = throughput_estimator->bitrate();
    ASSERT_TRUE(throughput.has_value());
    EXPECT_EQ(throughput.value(), send_rate);
  }

  // Delayed packet arrives ~1 second after it should have.
  // Since the window is 500 ms, the delayed packet was sent ~500
  // ms before the second oldest packet. However, the send rate
  // should not drop.
  delayed_packets.front().receive_time = recv_clock_;
  throughput_estimator->IncomingPacketFeedbackVector(delayed_packets);
  auto throughput = throughput_estimator->bitrate();
  ASSERT_TRUE(throughput.has_value());
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              send_rate.bytes_per_sec<double>(),
              0.05 * send_rate.bytes_per_sec<double>());  // Allow 5% error

  // Thoughput should stay stable.
  for (int i = 0; i < 10; i++) {
    std::vector<PacketResult> packet_feedback =
        CreateFeedbackVector(10, default_packet_size_, send_rate, recv_rate);
    throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
    auto throughput = throughput_estimator->bitrate();
    ASSERT_TRUE(throughput.has_value());
    EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
                send_rate.bytes_per_sec<double>(),
                0.05 * send_rate.bytes_per_sec<double>());  // Allow 5% error
  }
}

TEST_F(RobustThroughputEstimatorTest, StreamPausedAndResumed) {
  webrtc::test::ScopedFieldTrials field_trials(
      "WebRTC-Bwe-RobustThroughputEstimatorSettings/"
      "enabled:true,window_packets:20,max_window_packets:500,"
      "window_duration:1000ms,max_window_duration:5000ms,"
      "required_packets:10,unacked_weight:1.0/");
  std::unique_ptr<RobustThroughputEstimator> throughput_estimator =
      CreateRobustThroughputEstimator();
  DataRate send_rate(DataRate::BytesPerSec(100000));
  DataRate recv_rate(DataRate::BytesPerSec(100000));

  std::vector<PacketResult> packet_feedback =
      CreateFeedbackVector(20, default_packet_size_, send_rate, recv_rate);
  throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
  auto throughput = throughput_estimator->bitrate();
  EXPECT_TRUE(throughput.has_value());
  double expected_bytes_per_sec = 100 * 1000.0;
  EXPECT_NEAR(throughput.value().bytes_per_sec<double>(),
              expected_bytes_per_sec,
              0.05 * expected_bytes_per_sec);  // Allow 5% error

  // No packets sent or feedback received for 60s.
  send_clock_ += TimeDelta::Seconds(60);
  recv_clock_ += TimeDelta::Seconds(60);

  // Resume sending packets at the same rate as before. The estimate
  // will initially be invalid, due to lack of recent data.
  packet_feedback =
      CreateFeedbackVector(5, default_packet_size_, send_rate, recv_rate);
  throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
  throughput = throughput_estimator->bitrate();
  EXPECT_FALSE(throughput.has_value());

  // But be back to the normal level once we have enough data.
  for (int i = 0; i < 4; ++i) {
    packet_feedback =
        CreateFeedbackVector(5, default_packet_size_, send_rate, recv_rate);
    throughput_estimator->IncomingPacketFeedbackVector(packet_feedback);
    throughput = throughput_estimator->bitrate();
    EXPECT_TRUE(throughput.has_value());
    EXPECT_EQ(throughput.value(), send_rate);
  }
}

}  // namespace webrtc
