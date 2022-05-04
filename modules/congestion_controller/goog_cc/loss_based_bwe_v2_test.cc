/*
 *  Copyright 2021 The WebRTC project authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/loss_based_bwe_v2.h"

#include <string>

#include "api/network_state_predictor.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/strings/string_builder.h"
#include "test/explicit_key_value_config.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using ::webrtc::test::ExplicitKeyValueConfig;

constexpr TimeDelta kObservationDurationLowerBound = TimeDelta::Millis(200);
constexpr TimeDelta kDelayedIncreaseWindow = TimeDelta::Millis(300);
constexpr double kMaxIncreaseFactor = 1.5;

std::string Config(bool enabled,
                   bool valid,
                   bool trendline_integration = false) {
  char buffer[1024];
  rtc::SimpleStringBuilder config_string(buffer);

  config_string << "WebRTC-Bwe-LossBasedBweV2/";

  if (enabled) {
    config_string << "Enabled:true";
  } else {
    config_string << "Enabled:false";
  }

  if (valid) {
    config_string << ",BwRampupUpperBoundFactor:1.2";
  } else {
    config_string << ",BwRampupUpperBoundFactor:0.0";
  }

  if (trendline_integration) {
    config_string << "TrendlineIntegrationEnabled:true";
  } else {
    config_string << "TrendlineIntegrationEnabled:false";
  }

  config_string
      << ",CandidateFactors:1.1|1.0|0.95,HigherBwBiasFactor:0.01,"
         "DelayBasedCandidate:true,"
         "InherentLossLowerBound:0.001,InherentLossUpperBoundBwBalance:14kbps,"
         "InherentLossUpperBoundOffset:0.9,InitialInherentLossEstimate:0.01,"
         "NewtonIterations:2,NewtonStepSize:0.4,ObservationWindowSize:3,"
         "SendingRateSmoothingFactor:0.01,"
         "InstantUpperBoundTemporalWeightFactor:0.97,"
         "InstantUpperBoundBwBalance:90kbps,"
         "InstantUpperBoundLossOffset:0.1,TemporalWeightFactor:0.98";

  config_string.AppendFormat(
      ",ObservationDurationLowerBound:%dms",
      static_cast<int>(kObservationDurationLowerBound.ms()));
  config_string.AppendFormat(",MaxIncreaseFactor:%f", kMaxIncreaseFactor);
  config_string.AppendFormat(",DelayedIncreaseWindow:%dms",
                             static_cast<int>(kDelayedIncreaseWindow.ms()));

  config_string << "/";

  return config_string.str();
}

TEST(LossBasedBweV2Test, EnabledWhenGivenValidConfigurationValues) {
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  EXPECT_TRUE(loss_based_bandwidth_estimator.IsEnabled());
}

TEST(LossBasedBweV2Test, DisabledWhenGivenDisabledConfiguration) {
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/false, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsEnabled());
}

TEST(LossBasedBweV2Test, DisabledWhenGivenNonValidConfigurationValues) {
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/false));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsEnabled());
}

TEST(LossBasedBweV2Test, DisabledWhenGivenNonPositiveCandidateFactor) {
  ExplicitKeyValueConfig key_value_config_negative_candidate_factor(
      "WebRTC-Bwe-LossBasedBweV2/Enabled:true,CandidateFactors:-1.3|1.1/");
  LossBasedBweV2 loss_based_bandwidth_estimator_1(
      &key_value_config_negative_candidate_factor);
  EXPECT_FALSE(loss_based_bandwidth_estimator_1.IsEnabled());

  ExplicitKeyValueConfig key_value_config_zero_candidate_factor(
      "WebRTC-Bwe-LossBasedBweV2/Enabled:true,CandidateFactors:0.0|1.1/");
  LossBasedBweV2 loss_based_bandwidth_estimator_2(
      &key_value_config_zero_candidate_factor);
  EXPECT_FALSE(loss_based_bandwidth_estimator_2.IsEnabled());
}

TEST(LossBasedBweV2Test,
     DisabledWhenGivenConfigurationThatDoesNotAllowGeneratingCandidates) {
  ExplicitKeyValueConfig key_value_config(
      "WebRTC-Bwe-LossBasedBweV2/"
      "Enabled:true,CandidateFactors:1.0,AckedRateCandidate:false,"
      "DelayBasedCandidate:false/");
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  EXPECT_FALSE(loss_based_bandwidth_estimator.IsEnabled());
}

TEST(LossBasedBweV2Test, BandwidthEstimateGivenInitializationAndThenFeedback) {
  PacketResult enough_feedback[2];
  enough_feedback[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_TRUE(loss_based_bandwidth_estimator.IsReady());
  EXPECT_TRUE(
      loss_based_bandwidth_estimator
          .GetBandwidthEstimate(/*delay_based_limit=*/DataRate::PlusInfinity())
          .IsFinite());
}

TEST(LossBasedBweV2Test, NoBandwidthEstimateGivenNoInitialization) {
  PacketResult enough_feedback[2];
  enough_feedback[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsReady());
  EXPECT_TRUE(
      loss_based_bandwidth_estimator
          .GetBandwidthEstimate(/*delay_based_limit=*/DataRate::PlusInfinity())
          .IsPlusInfinity());
}

TEST(LossBasedBweV2Test, NoBandwidthEstimateGivenNotEnoughFeedback) {
  // Create packet results where the observation duration is less than the lower
  // bound.
  PacketResult not_enough_feedback[2];
  not_enough_feedback[0].sent_packet.size = DataSize::Bytes(15'000);
  not_enough_feedback[1].sent_packet.size = DataSize::Bytes(15'000);
  not_enough_feedback[0].sent_packet.send_time = Timestamp::Zero();
  not_enough_feedback[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound / 2;
  not_enough_feedback[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound / 2;
  not_enough_feedback[1].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsReady());
  EXPECT_TRUE(
      loss_based_bandwidth_estimator
          .GetBandwidthEstimate(/*delay_based_limit=*/DataRate::PlusInfinity())
          .IsPlusInfinity());

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      not_enough_feedback, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsReady());
  EXPECT_TRUE(
      loss_based_bandwidth_estimator
          .GetBandwidthEstimate(/*delay_based_limit=*/DataRate::PlusInfinity())
          .IsPlusInfinity());
}

TEST(LossBasedBweV2Test,
     SetValueIsTheEstimateUntilAdditionalFeedbackHasBeenReceived) {
  PacketResult enough_feedback_1[2];
  PacketResult enough_feedback_2[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[0].receive_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_2[1].receive_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_NE(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));

  EXPECT_EQ(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_NE(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));
}

TEST(LossBasedBweV2Test,
     SetAcknowledgedBitrateOnlyAffectsTheBweWhenAdditionalFeedbackIsGiven) {
  PacketResult enough_feedback_1[2];
  PacketResult enough_feedback_2[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[0].receive_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_2[1].receive_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator_1(&key_value_config);
  LossBasedBweV2 loss_based_bandwidth_estimator_2(&key_value_config);

  loss_based_bandwidth_estimator_1.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator_2.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator_1.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  loss_based_bandwidth_estimator_2.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_EQ(loss_based_bandwidth_estimator_1.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(660));

  loss_based_bandwidth_estimator_1.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(600));

  EXPECT_EQ(loss_based_bandwidth_estimator_1.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(660));

  loss_based_bandwidth_estimator_1.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  loss_based_bandwidth_estimator_2.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_NE(loss_based_bandwidth_estimator_1.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            loss_based_bandwidth_estimator_2.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()));
}

TEST(LossBasedBweV2Test,
     BandwidthEstimateIsCappedToBeTcpFairGivenTooHighLossRate) {
  PacketResult enough_feedback_no_received_packets[2];
  enough_feedback_no_received_packets[0].sent_packet.size =
      DataSize::Bytes(15'000);
  enough_feedback_no_received_packets[1].sent_packet.size =
      DataSize::Bytes(15'000);
  enough_feedback_no_received_packets[0].sent_packet.send_time =
      Timestamp::Zero();
  enough_feedback_no_received_packets[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_no_received_packets[0].receive_time =
      Timestamp::PlusInfinity();
  enough_feedback_no_received_packets[1].receive_time =
      Timestamp::PlusInfinity();

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_no_received_packets, DataRate::PlusInfinity(),
      BandwidthUsage::kBwNormal);

  EXPECT_EQ(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(100));
}

// When network is overusing and flag `BackoffWhenOverusing` is true,
// the bandwidth estimate is forced to decrease even if there is no loss yet.
TEST(LossBasedBweV2Test, BandwidthEstimateDecreasesWhenOverusing) {
  PacketResult enough_feedback_1[2];
  PacketResult enough_feedback_2[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time = Timestamp::PlusInfinity();
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[0].receive_time = Timestamp::PlusInfinity();
  enough_feedback_2[1].receive_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(300));
  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(),
      BandwidthUsage::kBwOverusing);
  EXPECT_LE(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  EXPECT_LE(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));
}

TEST(LossBasedBweV2Test, BandwidthEstimateIncreasesWhenUnderusing) {
  PacketResult enough_feedback_1[2];
  PacketResult enough_feedback_2[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[0].receive_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_2[1].receive_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(),
      BandwidthUsage::kBwUnderusing);
  EXPECT_GT(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  EXPECT_GT(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));
}

// When network is underusing, estimate can increase but never be higher than
// the delay based estimate.
TEST(LossBasedBweV2Test,
     BandwidthEstimateCappedByDelayBasedEstimateWhenUnderusing) {
  PacketResult enough_feedback_1[2];
  PacketResult enough_feedback_2[2];
  // Create two packet results, network is in normal state, 100% packets are
  // received, and no delay increase.
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[0].receive_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_2[1].receive_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(),
      BandwidthUsage::kBwUnderusing);
  // If the delay based estimate is infinity, then loss based estimate increases
  // and not bounded by delay based estimate.
  EXPECT_GT(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  // If the delay based estimate is not infinity, then loss based estimate is
  // bounded by delay based estimate.
  EXPECT_EQ(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::KilobitsPerSec(500)),
            DataRate::KilobitsPerSec(500));
}

// When loss based bwe receives a strong signal of overusing and an increase in
// loss rate, it should acked bitrate for emegency backoff.
TEST(LossBasedBweV2Test, UseAckedBitrateForEmegencyBackOff) {
  PacketResult enough_feedback_1[2];
  PacketResult enough_feedback_2[2];
  // Create two packet results, first packet has 50% loss rate, second packet
  // has 100% loss rate.
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time = Timestamp::PlusInfinity();
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[0].receive_time = Timestamp::PlusInfinity();
  enough_feedback_2[1].receive_time = Timestamp::PlusInfinity();

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  DataRate acked_bitrate = DataRate::KilobitsPerSec(300);
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(acked_bitrate);
  // Update estimate when network is overusing, and 50% loss rate.
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(),
      BandwidthUsage::kBwOverusing);
  // Update estimate again when network is continuously overusing, and 100%
  // loss rate.
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(),
      BandwidthUsage::kBwOverusing);
  // The estimate bitrate now is backed off based on acked bitrate.
  EXPECT_LE(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            acked_bitrate);
}

TEST(LossBasedBweV2Test, NoUpdateIfObservationDurationUnchanged) {
  PacketResult enough_feedback_1[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  DataRate acked_bitrate = DataRate::KilobitsPerSec(300);
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(acked_bitrate);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  DataRate estimate_1 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());

  // Use the same feedback and check if the estimate is unchanged.
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  DataRate estimate_2 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());
  EXPECT_LE(estimate_2, estimate_1);
}

TEST(LossBasedBweV2Test, NoUpdateIfObservationDurationIsSmallAndNetworkNormal) {
  PacketResult enough_feedback_1[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  DataRate acked_bitrate = DataRate::KilobitsPerSec(300);
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(acked_bitrate);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  DataRate estimate_1 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());

  // Create a feedback within ObservationDurationLowerBound and check the
  // estimate unchanged.
  PacketResult enough_feedback_2[2];
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound - TimeDelta::Millis(1);
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound -
      TimeDelta::Millis(1);
  enough_feedback_2[0].receive_time = Timestamp::Zero() +
                                      2 * kObservationDurationLowerBound -
                                      TimeDelta::Millis(1);
  enough_feedback_2[1].receive_time = Timestamp::Zero() +
                                      3 * kObservationDurationLowerBound -
                                      TimeDelta::Millis(1);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  DataRate estimate_2 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());
  EXPECT_LE(estimate_2, estimate_1);
}

TEST(LossBasedBweV2Test,
     NoUpdateIfObservationDurationIsSmallAndNetworkUnderusing) {
  PacketResult enough_feedback_1[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true, /*trendline_integration=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  DataRate acked_bitrate = DataRate::KilobitsPerSec(300);
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(acked_bitrate);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  DataRate estimate_1 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());

  // Create a feedback within ObservationDurationLowerBound and check the
  // estimate is unchanged because the network is underusing.
  PacketResult enough_feedback_2[2];
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound - TimeDelta::Millis(1);
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound -
      TimeDelta::Millis(1);
  enough_feedback_2[0].receive_time = Timestamp::Zero() +
                                      2 * kObservationDurationLowerBound -
                                      TimeDelta::Millis(1);
  enough_feedback_2[1].receive_time = Timestamp::Zero() +
                                      3 * kObservationDurationLowerBound -
                                      TimeDelta::Millis(1);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(),
      BandwidthUsage::kBwUnderusing);
  DataRate estimate_2 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());
  EXPECT_LE(estimate_2, estimate_1);
}

TEST(LossBasedBweV2Test,
     UpdateIfObservationDurationIsSmallAndNetworkOverusing) {
  PacketResult enough_feedback_1[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true, /*trendline_integration=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  DataRate acked_bitrate = DataRate::KilobitsPerSec(300);
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(acked_bitrate);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  DataRate estimate_1 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());

  // Create a feedback within ObservationDurationLowerBound and check the
  // estimate is changed because the network is overusing.
  PacketResult enough_feedback_2[2];
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound - TimeDelta::Millis(1);
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound -
      TimeDelta::Millis(1);
  enough_feedback_2[0].receive_time = Timestamp::PlusInfinity();
  enough_feedback_2[1].receive_time = Timestamp::PlusInfinity();

  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(100));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(),
      BandwidthUsage::kBwOverusing);
  DataRate estimate_2 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());
  EXPECT_LE(estimate_2, estimate_1);
}

TEST(LossBasedBweV2Test, IncreaseToDelayBasedEstimate) {
  PacketResult enough_feedback_1[2];
  PacketResult enough_feedback_2[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[0].receive_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_2[1].receive_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  DataRate current_estimate = DataRate::KilobitsPerSec(600);
  DataRate delay_based_estimate = DataRate::KilobitsPerSec(5000);
  loss_based_bandwidth_estimator.SetBandwidthEstimate(current_estimate);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, delay_based_estimate, BandwidthUsage::kBwUnderusing);
  EXPECT_EQ(
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate),
      delay_based_estimate);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, delay_based_estimate, BandwidthUsage::kBwUnderusing);
  EXPECT_EQ(
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate),
      delay_based_estimate);
}

// After loss based bwe backs off, the next estimate is capped by
// MaxIncreaseFactor * current estimate.
TEST(LossBasedBweV2Test, IncreaseByMaxIncreaseFactorAfterLossBasedBweBacksOff) {
  PacketResult enough_feedback_1[2];
  PacketResult enough_feedback_2[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + 1 * kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[0].receive_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_2[1].receive_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(300));
  DataRate delay_based_estimate = DataRate::KilobitsPerSec(5000);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, delay_based_estimate, BandwidthUsage::kBwNormal);
  // The estimate is bouned because the acknowledged bitrate is low.
  DataRate current_estimate =
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate);
  EXPECT_LT(current_estimate, delay_based_estimate);

  // Increase the acknowledged bitrate to make sure that the estimate is not
  // capped too low.
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(5000));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, delay_based_estimate, BandwidthUsage::kBwUnderusing);

  // The new estimate is capped by current_estimate * kMaxIncreaseFactor
  DataRate new_estimate =
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate);
  EXPECT_EQ(new_estimate, current_estimate * kMaxIncreaseFactor);
  EXPECT_LE(new_estimate, delay_based_estimate);
}

// After loss based bwe backs off, the estimate is bounded during the delayed
// window.
TEST(LossBasedBweV2Test,
     EstimateBitrateIsBoundedDuringDelayedWindowAfterLossBasedBweBacksOff) {
  PacketResult enough_feedback_1[2];
  PacketResult enough_feedback_2[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + 1 * kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[0].receive_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_2[1].receive_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(300));
  DataRate delay_based_estimate = DataRate::KilobitsPerSec(5000);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, delay_based_estimate, BandwidthUsage::kBwNormal);
  // The estimate is bouned because the acknowledged bitrate is low.
  DataRate current_estimate =
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate);
  EXPECT_LT(current_estimate, delay_based_estimate);

  // Increase the acknowledged bitrate to make sure that the estimate is not
  // capped too low.
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(5000));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, delay_based_estimate, BandwidthUsage::kBwUnderusing);

  // The next estimate is capped by current_estimate * kMaxIncreaseFactor
  DataRate next_estimate =
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate);
  EXPECT_EQ(next_estimate, current_estimate * kMaxIncreaseFactor);
  EXPECT_LE(next_estimate, delay_based_estimate);

  PacketResult enough_feedback_3[2];
  enough_feedback_3[0].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_3[1].sent_packet.send_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;
  enough_feedback_3[0].receive_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;
  enough_feedback_3[1].receive_time =
      Timestamp::Zero() + 5 * kObservationDurationLowerBound;
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_3, delay_based_estimate, BandwidthUsage::kBwUnderusing);
  // The latest estimate is the same as the previous estimate since it is still
  // in the DelayedIncreaseWindow
  EXPECT_EQ(
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate),
      next_estimate);
}

// The estimate is not bounded after the delayed window.
TEST(LossBasedBweV2Test, KeepIncreasingEstimateAfterDelayedWindow) {
  PacketResult enough_feedback_1[2];
  PacketResult enough_feedback_2[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + 1 * kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[0].receive_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_2[1].receive_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(300));
  DataRate delay_based_estimate = DataRate::KilobitsPerSec(5000);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, delay_based_estimate, BandwidthUsage::kBwNormal);
  // The estimate is bouned because the acknowledged bitrate is low.
  DataRate current_estimate =
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate);
  EXPECT_LT(current_estimate, delay_based_estimate);

  // Increase the acknowledged bitrate to make sure that the estimate is not
  // capped too low.
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(5000));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, delay_based_estimate, BandwidthUsage::kBwUnderusing);

  // The next estimate is capped by current_estimate * kMaxIncreaseFactor
  DataRate next_estimate =
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate);
  EXPECT_EQ(next_estimate, current_estimate * kMaxIncreaseFactor);
  EXPECT_LE(next_estimate, delay_based_estimate);

  PacketResult enough_feedback_3[2];
  enough_feedback_3[0].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound +
      kDelayedIncreaseWindow;
  enough_feedback_3[1].sent_packet.send_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound +
      kDelayedIncreaseWindow;
  enough_feedback_3[0].receive_time = Timestamp::Zero() +
                                      4 * kObservationDurationLowerBound +
                                      kDelayedIncreaseWindow;
  enough_feedback_3[1].receive_time = Timestamp::Zero() +
                                      5 * kObservationDurationLowerBound +
                                      kDelayedIncreaseWindow;
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_3, delay_based_estimate, BandwidthUsage::kBwUnderusing);
  // The estimate can continue increasing after the DelayedIncreaseWindow
  EXPECT_GE(
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate),
      next_estimate);
}

}  // namespace
}  // namespace webrtc
