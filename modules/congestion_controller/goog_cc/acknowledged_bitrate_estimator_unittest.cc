/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/transport/field_trial_based_config.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/goog_cc/bitrate_estimator.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::InSequence;
using ::testing::Return;

namespace webrtc {

namespace {

constexpr int64_t kFirstArrivalTimeMs = 10;
constexpr int64_t kFirstSendTimeMs = 10;
constexpr uint16_t kSequenceNumber = 1;
constexpr size_t kPayloadSize = 10;

class MockBitrateEstimator : public BitrateEstimator {
 public:
  using BitrateEstimator::BitrateEstimator;
  MOCK_METHOD(void,
              Update,
              (Timestamp at_time, DataSize data_size, bool in_alr),
              (override));
  MOCK_METHOD(std::optional<DataRate>, bitrate, (), (const, override));
  MOCK_METHOD(void, ExpectFastRateChange, (), (override));
};

struct AcknowledgedBitrateEstimatorTestStates {
  FieldTrialBasedConfig field_trial_config;
  std::unique_ptr<AcknowledgedBitrateEstimator> acknowledged_bitrate_estimator;
  MockBitrateEstimator* mock_bitrate_estimator;
};

AcknowledgedBitrateEstimatorTestStates CreateTestStates(
    bool use_real_bitrate_estimator = false) {
  AcknowledgedBitrateEstimatorTestStates states;
  if (use_real_bitrate_estimator) {
    auto bitrate_estimator =
        std::make_unique<webrtc::BitrateEstimator>(&states.field_trial_config);
    states.mock_bitrate_estimator = nullptr;
    states.acknowledged_bitrate_estimator =
        std::make_unique<AcknowledgedBitrateEstimator>(
            &states.field_trial_config, std::move(bitrate_estimator));
  } else {
    auto mock_bitrate_estimator =
        std::make_unique<MockBitrateEstimator>(&states.field_trial_config);
    states.mock_bitrate_estimator = mock_bitrate_estimator.get();
    states.acknowledged_bitrate_estimator =
        std::make_unique<AcknowledgedBitrateEstimator>(
            &states.field_trial_config, std::move(mock_bitrate_estimator));
  }
  return states;
}

std::vector<PacketResult> CreateFeedbackVector() {
  std::vector<PacketResult> packet_feedback_vector(2);
  packet_feedback_vector[0].receive_time =
      Timestamp::Millis(kFirstArrivalTimeMs);
  packet_feedback_vector[0].sent_packet.send_time =
      Timestamp::Millis(kFirstSendTimeMs);
  packet_feedback_vector[0].sent_packet.sequence_number = kSequenceNumber;
  packet_feedback_vector[0].sent_packet.size = DataSize::Bytes(kPayloadSize);
  packet_feedback_vector[1].receive_time =
      Timestamp::Millis(kFirstArrivalTimeMs + 10);
  packet_feedback_vector[1].sent_packet.send_time =
      Timestamp::Millis(kFirstSendTimeMs + 10);
  packet_feedback_vector[1].sent_packet.sequence_number = kSequenceNumber;
  packet_feedback_vector[1].sent_packet.size =
      DataSize::Bytes(kPayloadSize + 10);
  return packet_feedback_vector;
}

}  // anonymous namespace

TEST(TestAcknowledgedBitrateEstimator, UpdateBandwidth) {
  auto states = CreateTestStates();
  auto packet_feedback_vector = CreateFeedbackVector();
  {
    InSequence dummy;
    EXPECT_CALL(*states.mock_bitrate_estimator,
                Update(packet_feedback_vector[0].receive_time,
                       packet_feedback_vector[0].sent_packet.size,
                       /*in_alr*/ false))
        .Times(1);
    EXPECT_CALL(*states.mock_bitrate_estimator,
                Update(packet_feedback_vector[1].receive_time,
                       packet_feedback_vector[1].sent_packet.size,
                       /*in_alr*/ false))
        .Times(1);
  }
  states.acknowledged_bitrate_estimator->IncomingPacketFeedbackVector(
      packet_feedback_vector);
}

TEST(TestAcknowledgedBitrateEstimator, ExpectFastRateChangeWhenLeftAlr) {
  auto states = CreateTestStates();
  auto packet_feedback_vector = CreateFeedbackVector();
  {
    InSequence dummy;
    EXPECT_CALL(*states.mock_bitrate_estimator,
                Update(packet_feedback_vector[0].receive_time,
                       packet_feedback_vector[0].sent_packet.size,
                       /*in_alr*/ false))
        .Times(1);
    EXPECT_CALL(*states.mock_bitrate_estimator, ExpectFastRateChange())
        .Times(1);
    EXPECT_CALL(*states.mock_bitrate_estimator,
                Update(packet_feedback_vector[1].receive_time,
                       packet_feedback_vector[1].sent_packet.size,
                       /*in_alr*/ false))
        .Times(1);
  }
  states.acknowledged_bitrate_estimator->SetAlrEndedTime(
      Timestamp::Millis(kFirstArrivalTimeMs + 1));
  states.acknowledged_bitrate_estimator->IncomingPacketFeedbackVector(
      packet_feedback_vector);
}

TEST(TestAcknowledgedBitrateEstimator, ReturnBitrate) {
  auto states = CreateTestStates();
  std::optional<DataRate> return_value = DataRate::KilobitsPerSec(42);
  EXPECT_CALL(*states.mock_bitrate_estimator, bitrate())
      .Times(1)
      .WillOnce(Return(return_value));
  EXPECT_EQ(return_value, states.acknowledged_bitrate_estimator->bitrate());
}

TEST(TestAcknowledgedBitrateEstimator, CorrectBitrateAfterFirstPacket) {
  auto states = CreateTestStates(/*use_real_bitrate_estimator=*/true);

  // Simulate 6 packets received every 100 ms starting at time 0 ms.
  Timestamp receive_time = Timestamp::Millis(0);
  DataSize packet_size = DataSize::Bytes(1000);  // 1000 bytes per packet

  // Send 6 packets to cover 500 ms
  for (uint8_t i = 1; i <= 6; ++i) {
    PacketResult packet_feedback;
    packet_feedback.receive_time = receive_time;
    packet_feedback.sent_packet.send_time = receive_time;
    packet_feedback.sent_packet.sequence_number = i;
    packet_feedback.sent_packet.size = packet_size;
    std::vector<PacketResult> packet_feedback_vector = {packet_feedback};
    states.acknowledged_bitrate_estimator->IncomingPacketFeedbackVector(
        packet_feedback_vector);
    receive_time += TimeDelta::Millis(100);
  }

  // Now (receive_time=600 ms) we should have a valid bitrate estimate
  // - sum_ includes bytes from packets 2 to 5 (4 packets * 1000 bytes)
  // - Time interval is 500 ms (from 100 ms to 600 ms)
  // - Expected bitrate: (8 * 4000 bytes) / 500 ms = 64 kbps
  std::optional<DataRate> bitrate =
      states.acknowledged_bitrate_estimator->bitrate();
  DataRate expected_bitrate = DataRate::KilobitsPerSec(64);
  EXPECT_EQ(bitrate->kbps(), expected_bitrate.kbps());
}

}  // namespace webrtc*/
