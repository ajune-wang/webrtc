/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <memory>

#include "modules/congestion_controller/probe_controller.h"
#include "network_control/include/network_types.h"
#include "network_control/include/test/network_message_test.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Field;
using testing::Matcher;
using testing::NiceMock;
using testing::Return;

using webrtc::ProbeClusterConfig;
using webrtc::signal::test::MockObserver;

namespace webrtc {
namespace test {

namespace {

constexpr int kMinBitrateBps = 100;
constexpr int kStartBitrateBps = 300;
constexpr int kMaxBitrateBps = 10000;

constexpr int kExponentialProbingTimeoutMs = 5000;

constexpr int kAlrProbeInterval = 5000;
constexpr int kAlrEndedTimeoutMs = 3000;
constexpr int kBitrateDropTimeoutMs = 5000;

inline Matcher<ProbeClusterConfig> DataRateEqBps(int bps) {
  return Field(&ProbeClusterConfig::target_data_rate, DataRate::bps(bps));
}

}  // namespace

class ProbeControllerTest : public ::testing::Test {
 protected:
  ProbeControllerTest() : clock_(100000000L) {
    probe_controller_.reset(new ProbeController(&cluster_handler_));
    NetworkAvailabilityJunction.Connect(
        &probe_controller_->NetworkAvailabilityReceiver);
  }
  ~ProbeControllerTest() override {
    NetworkAvailabilityJunction.Disconnect(
        &probe_controller_->NetworkAvailabilityReceiver);
  }

  void SetNetworkAvailable(bool available) {
    NetworkAvailability msg;
    msg.at_time = Timestamp::ms(clock_.TimeInMicroseconds());
    msg.network_available = available;
    NetworkAvailabilityJunction.OnMessage(msg);
  }

  int64_t NowMs() { return clock_.TimeInMilliseconds(); }

  SimulatedClock clock_;
  NiceMock<MockObserver<ProbeClusterConfig>> cluster_handler_;
  NetworkAvailability::SimpleJunction NetworkAvailabilityJunction;
  std::unique_ptr<ProbeController> probe_controller_;
};

TEST_F(ProbeControllerTest, InitiatesProbingAtStart) {
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(AtLeast(2));
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());
}

TEST_F(ProbeControllerTest, ProbeOnlyWhenNetworkIsUp) {
  SetNetworkAvailable(false);
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(0);
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());

  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(AtLeast(2));
  SetNetworkAvailable(true);
}

TEST_F(ProbeControllerTest, InitiatesProbingOnMaxBitrateIncrease) {
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(AtLeast(2));
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());
  // Long enough to time out exponential probing.
  clock_.AdvanceTimeMilliseconds(kExponentialProbingTimeoutMs);
  probe_controller_->SetEstimatedBitrate(kStartBitrateBps, NowMs());
  probe_controller_->Process(NowMs());

  EXPECT_CALL(cluster_handler_, OnMessage(DataRateEqBps(kMaxBitrateBps + 100)));
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps + 100, NowMs());
}

TEST_F(ProbeControllerTest, InitiatesProbingOnMaxBitrateIncreaseAtMaxBitrate) {
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(AtLeast(2));
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());
  // Long enough to time out exponential probing.
  clock_.AdvanceTimeMilliseconds(kExponentialProbingTimeoutMs);
  probe_controller_->SetEstimatedBitrate(kStartBitrateBps, NowMs());
  probe_controller_->Process(NowMs());

  probe_controller_->SetEstimatedBitrate(kMaxBitrateBps, NowMs());
  EXPECT_CALL(cluster_handler_, OnMessage(DataRateEqBps(kMaxBitrateBps + 100)));
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps + 100, NowMs());
}

TEST_F(ProbeControllerTest, TestExponentialProbing) {
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());

  // Repeated probe should only be sent when estimated bitrate climbs above
  // 0.7 * 6 * kStartBitrateBps = 1260.
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(0);
  probe_controller_->SetEstimatedBitrate(1000, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);

  EXPECT_CALL(cluster_handler_, OnMessage(DataRateEqBps(2 * 1800)));
  probe_controller_->SetEstimatedBitrate(1800, NowMs());
}

TEST_F(ProbeControllerTest, TestExponentialProbingTimeout) {
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());

  // Advance far enough to cause a time out in waiting for probing result.
  clock_.AdvanceTimeMilliseconds(kExponentialProbingTimeoutMs);
  probe_controller_->Process(NowMs());

  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(0);
  probe_controller_->SetEstimatedBitrate(1800, NowMs());
}

TEST_F(ProbeControllerTest, RequestProbeInAlr) {
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(2);
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);
  EXPECT_CALL(cluster_handler_, OnMessage(DataRateEqBps(0.85 * 500))).Times(1);
  probe_controller_->SetAlrStartTimeMs(clock_.TimeInMilliseconds());
  clock_.AdvanceTimeMilliseconds(kAlrProbeInterval + 1);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(250, NowMs());
  probe_controller_->RequestProbe(NowMs());
}

TEST_F(ProbeControllerTest, RequestProbeWhenAlrEndedRecently) {
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(2);
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);
  EXPECT_CALL(cluster_handler_, OnMessage(DataRateEqBps(0.85 * 500))).Times(1);
  probe_controller_->SetAlrStartTimeMs(rtc::nullopt);
  clock_.AdvanceTimeMilliseconds(kAlrProbeInterval + 1);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(250, NowMs());
  probe_controller_->SetAlrEndedTimeMs(clock_.TimeInMilliseconds());
  clock_.AdvanceTimeMilliseconds(kAlrEndedTimeoutMs - 1);
  probe_controller_->RequestProbe(NowMs());
}

TEST_F(ProbeControllerTest, RequestProbeWhenAlrNotEndedRecently) {
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(2);
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(0);
  probe_controller_->SetAlrStartTimeMs(rtc::nullopt);
  clock_.AdvanceTimeMilliseconds(kAlrProbeInterval + 1);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(250, NowMs());
  probe_controller_->SetAlrEndedTimeMs(clock_.TimeInMilliseconds());
  clock_.AdvanceTimeMilliseconds(kAlrEndedTimeoutMs + 1);
  probe_controller_->RequestProbe(NowMs());
}

TEST_F(ProbeControllerTest, RequestProbeWhenBweDropNotRecent) {
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(2);
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(0);
  probe_controller_->SetAlrStartTimeMs(clock_.TimeInMilliseconds());
  clock_.AdvanceTimeMilliseconds(kAlrProbeInterval + 1);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(250, NowMs());
  clock_.AdvanceTimeMilliseconds(kBitrateDropTimeoutMs + 1);
  probe_controller_->RequestProbe(NowMs());
}

TEST_F(ProbeControllerTest, PeriodicProbing) {
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(2);
  probe_controller_->EnablePeriodicAlrProbing(true);
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);

  int64_t start_time = clock_.TimeInMilliseconds();

  // Expect the controller to send a new probe after 5s has passed.
  EXPECT_CALL(cluster_handler_, OnMessage(DataRateEqBps(1000))).Times(1);
  probe_controller_->SetAlrStartTimeMs(start_time);
  clock_.AdvanceTimeMilliseconds(5000);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);

  // The following probe should be sent at 10s into ALR.
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(0);
  probe_controller_->SetAlrStartTimeMs(start_time);
  clock_.AdvanceTimeMilliseconds(4000);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);

  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(1);
  probe_controller_->SetAlrStartTimeMs(start_time);
  clock_.AdvanceTimeMilliseconds(1000);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);
}

TEST_F(ProbeControllerTest, PeriodicProbingAfterReset) {
  NetworkAvailabilityJunction.Disconnect(
      &probe_controller_->NetworkAvailabilityReceiver);
  NiceMock<MockObserver<ProbeClusterConfig>> local_handler;
  probe_controller_.reset(new ProbeController(&local_handler));
  NetworkAvailabilityJunction.Connect(
      &probe_controller_->NetworkAvailabilityReceiver);
  int64_t alr_start_time = clock_.TimeInMilliseconds();

  probe_controller_->SetAlrStartTimeMs(alr_start_time);
  EXPECT_CALL(local_handler, OnMessage(_)).Times(2);
  probe_controller_->EnablePeriodicAlrProbing(true);
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());
  probe_controller_->Reset(NowMs());

  clock_.AdvanceTimeMilliseconds(10000);
  probe_controller_->Process(NowMs());

  EXPECT_CALL(local_handler, OnMessage(_)).Times(2);
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps, NowMs());

  // Make sure we use |kStartBitrateBps| as the estimated bitrate
  // until SetEstimatedBitrate is called with an updated estimate.
  clock_.AdvanceTimeMilliseconds(10000);
  EXPECT_CALL(local_handler, OnMessage(DataRateEqBps(kStartBitrateBps * 2)));
  probe_controller_->Process(NowMs());
}

TEST_F(ProbeControllerTest, TestExponentialProbingOverflow) {
  const int64_t kMbpsMultiplier = 1000000;
  probe_controller_->SetBitrates(kMinBitrateBps, 10 * kMbpsMultiplier,
                                 100 * kMbpsMultiplier, NowMs());

  // Verify that probe bitrate is capped at the specified max bitrate
  EXPECT_CALL(cluster_handler_,
              OnMessage(DataRateEqBps(100 * kMbpsMultiplier)));
  probe_controller_->SetEstimatedBitrate(60 * kMbpsMultiplier, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);

  // Verify that repeated probes aren't sent.
  EXPECT_CALL(cluster_handler_, OnMessage(_)).Times(0);
  probe_controller_->SetEstimatedBitrate(100 * kMbpsMultiplier, NowMs());
}

}  // namespace test
}  // namespace webrtc
