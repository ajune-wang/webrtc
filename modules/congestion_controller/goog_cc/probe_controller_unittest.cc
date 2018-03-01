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

#include "modules/congestion_controller/goog_cc/probe_controller.h"
#include "modules/congestion_controller/network_control/include/network_types.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Field;
using testing::Matcher;
using testing::NiceMock;
using testing::StrictMock;
using testing::Return;

namespace webrtc {
namespace webrtc_cc {
namespace test {

namespace {

constexpr int kStartBitrateBps = 300;
constexpr int kMaxBitrateBps = 10000;

constexpr int kExponentialProbingTimeoutMs = 5000;

constexpr int kAlrProbeInterval = 5000;
constexpr int kAlrEndedTimeoutMs = 3000;
constexpr int kBitrateDropTimeoutMs = 5000;

inline Matcher<ProbeClusterConfig> DataRateEqBps(int bps) {
  return Field(&ProbeClusterConfig::target_data_rate, DataRate::bps(bps));
}
class MockNetworkControllerObserver : public NetworkControllerObserver {
 public:
  MOCK_METHOD1(OnCongestionWindow, void(CongestionWindow));
  MOCK_METHOD1(OnPacerConfig, void(PacerConfig));
  MOCK_METHOD1(OnProbeClusterConfig, void(ProbeClusterConfig));
  MOCK_METHOD1(OnTargetTransferRate, void(TargetTransferRate));
};
}  // namespace

class ProbeControllerTest : public ::testing::Test {
 protected:
  ProbeControllerTest() : clock_(100000000L) {
    EXPECT_CALL(cluster_handler_, OnProbeClusterConfig(_)).Times(AtLeast(2));
    probe_controller_.reset(
        new ProbeController(&cluster_handler_, NowMs(), kStartBitrateBps,
                            kStartBitrateBps, kMaxBitrateBps, false));
  }
  ~ProbeControllerTest() override {}

  int64_t NowMs() { return clock_.TimeInMilliseconds(); }

  SimulatedClock clock_;
  NiceMock<MockNetworkControllerObserver> cluster_handler_;
  std::unique_ptr<ProbeController> probe_controller_;
};

TEST_F(ProbeControllerTest, InitiatesProbingAtStart) {
  // Only running fixture
}

TEST_F(ProbeControllerTest, InitiatesProbingOnMaxBitrateIncrease) {
  // Long enough to time out exponential probing.
  clock_.AdvanceTimeMilliseconds(kExponentialProbingTimeoutMs);
  probe_controller_->SetEstimatedBitrate(kStartBitrateBps, NowMs());
  probe_controller_->Process(NowMs());

  EXPECT_CALL(cluster_handler_,
              OnProbeClusterConfig(DataRateEqBps(kMaxBitrateBps + 100)));
  probe_controller_->UpdateMaxBitrate(kMaxBitrateBps + 100, NowMs());
}

TEST_F(ProbeControllerTest, InitiatesProbingOnMaxBitrateIncreaseAtMaxBitrate) {
  // Long enough to time out exponential probing.
  clock_.AdvanceTimeMilliseconds(kExponentialProbingTimeoutMs);
  probe_controller_->SetEstimatedBitrate(kStartBitrateBps, NowMs());
  probe_controller_->Process(NowMs());

  probe_controller_->SetEstimatedBitrate(kMaxBitrateBps, NowMs());
  EXPECT_CALL(cluster_handler_,
              OnProbeClusterConfig(DataRateEqBps(kMaxBitrateBps + 100)));
  probe_controller_->UpdateMaxBitrate(kMaxBitrateBps + 100, NowMs());
}

TEST_F(ProbeControllerTest, TestExponentialProbing) {
  // Repeated probe should only be sent when estimated bitrate climbs above
  // 0.7 * 6 * kStartBitrateBps = 1260.
  EXPECT_CALL(cluster_handler_, OnProbeClusterConfig(_)).Times(0);
  probe_controller_->SetEstimatedBitrate(1000, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);

  EXPECT_CALL(cluster_handler_, OnProbeClusterConfig(DataRateEqBps(2 * 1800)));
  probe_controller_->SetEstimatedBitrate(1800, NowMs());
}

TEST_F(ProbeControllerTest, TestExponentialProbingTimeout) {
  // Advance far enough to cause a time out in waiting for probing result.
  clock_.AdvanceTimeMilliseconds(kExponentialProbingTimeoutMs);
  probe_controller_->Process(NowMs());

  EXPECT_CALL(cluster_handler_, OnProbeClusterConfig(_)).Times(0);
  probe_controller_->SetEstimatedBitrate(1800, NowMs());
}

TEST_F(ProbeControllerTest, RequestProbeInAlr) {
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);
  EXPECT_CALL(cluster_handler_, OnProbeClusterConfig(DataRateEqBps(0.85 * 500)))
      .Times(1);
  probe_controller_->SetAlrStartTimeMs(clock_.TimeInMilliseconds());
  clock_.AdvanceTimeMilliseconds(kAlrProbeInterval + 1);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(250, NowMs());
  probe_controller_->RequestProbe(NowMs());
}

TEST_F(ProbeControllerTest, RequestProbeWhenAlrEndedRecently) {
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);
  EXPECT_CALL(cluster_handler_, OnProbeClusterConfig(DataRateEqBps(0.85 * 500)))
      .Times(1);
  probe_controller_->SetAlrStartTimeMs(rtc::nullopt);
  clock_.AdvanceTimeMilliseconds(kAlrProbeInterval + 1);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(250, NowMs());
  probe_controller_->SetAlrEndedTimeMs(clock_.TimeInMilliseconds());
  clock_.AdvanceTimeMilliseconds(kAlrEndedTimeoutMs - 1);
  probe_controller_->RequestProbe(NowMs());
}

TEST_F(ProbeControllerTest, RequestProbeWhenAlrNotEndedRecently) {
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);
  EXPECT_CALL(cluster_handler_, OnProbeClusterConfig(_)).Times(0);
  probe_controller_->SetAlrStartTimeMs(rtc::nullopt);
  clock_.AdvanceTimeMilliseconds(kAlrProbeInterval + 1);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(250, NowMs());
  probe_controller_->SetAlrEndedTimeMs(clock_.TimeInMilliseconds());
  clock_.AdvanceTimeMilliseconds(kAlrEndedTimeoutMs + 1);
  probe_controller_->RequestProbe(NowMs());
}

TEST_F(ProbeControllerTest, RequestProbeWhenBweDropNotRecent) {
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);
  EXPECT_CALL(cluster_handler_, OnProbeClusterConfig(_)).Times(0);
  probe_controller_->SetAlrStartTimeMs(clock_.TimeInMilliseconds());
  clock_.AdvanceTimeMilliseconds(kAlrProbeInterval + 1);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(250, NowMs());
  clock_.AdvanceTimeMilliseconds(kBitrateDropTimeoutMs + 1);
  probe_controller_->RequestProbe(NowMs());
}

TEST_F(ProbeControllerTest, PeriodicProbing) {
  probe_controller_->EnablePeriodicAlrProbing(true);
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);

  int64_t start_time = clock_.TimeInMilliseconds();

  // Expect the controller to send a new probe after 5s has passed.
  EXPECT_CALL(cluster_handler_, OnProbeClusterConfig(DataRateEqBps(1000)))
      .Times(1);
  probe_controller_->SetAlrStartTimeMs(start_time);
  clock_.AdvanceTimeMilliseconds(5000);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);

  // The following probe should be sent at 10s into ALR.
  EXPECT_CALL(cluster_handler_, OnProbeClusterConfig(_)).Times(0);
  probe_controller_->SetAlrStartTimeMs(start_time);
  clock_.AdvanceTimeMilliseconds(4000);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);

  EXPECT_CALL(cluster_handler_, OnProbeClusterConfig(_)).Times(1);
  probe_controller_->SetAlrStartTimeMs(start_time);
  clock_.AdvanceTimeMilliseconds(1000);
  probe_controller_->Process(NowMs());
  probe_controller_->SetEstimatedBitrate(500, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);
}

TEST_F(ProbeControllerTest, PeriodicProbingAfterReset) {
  StrictMock<MockNetworkControllerObserver> local_handler;
  EXPECT_CALL(local_handler, OnProbeClusterConfig(_)).Times(2);
  probe_controller_.reset(
      new ProbeController(&local_handler, NowMs(), kStartBitrateBps,
                          kStartBitrateBps, kMaxBitrateBps, false));
  int64_t alr_start_time = clock_.TimeInMilliseconds();

  probe_controller_->SetAlrStartTimeMs(alr_start_time);
  EXPECT_CALL(local_handler, OnProbeClusterConfig(_)).Times(1);
  probe_controller_->EnablePeriodicAlrProbing(true);
  clock_.AdvanceTimeMilliseconds(10000);
  probe_controller_->Process(NowMs());

  EXPECT_CALL(local_handler, OnProbeClusterConfig(_)).Times(2);
  probe_controller_.reset(
      new ProbeController(&local_handler, NowMs(), kStartBitrateBps,
                          kStartBitrateBps, kMaxBitrateBps, true));
  probe_controller_->SetAlrStartTimeMs(alr_start_time);

  // Make sure we use |kStartBitrateBps| as the estimated bitrate
  // until SetEstimatedBitrate is called with an updated estimate.
  clock_.AdvanceTimeMilliseconds(10000);
  EXPECT_CALL(local_handler,
              OnProbeClusterConfig(DataRateEqBps(kStartBitrateBps * 2)));
  probe_controller_->Process(NowMs());
}

TEST_F(ProbeControllerTest, TestExponentialProbingOverflow) {
  const int64_t kMbpsMultiplier = 1000000;
  probe_controller_.reset(
      new ProbeController(&cluster_handler_, NowMs(), 10 * kMbpsMultiplier,
                          10 * kMbpsMultiplier, 100 * kMbpsMultiplier, false));

  // Verify that probe bitrate is capped at the specified max bitrate.
  EXPECT_CALL(cluster_handler_,
              OnProbeClusterConfig(DataRateEqBps(100 * kMbpsMultiplier)));
  probe_controller_->SetEstimatedBitrate(60 * kMbpsMultiplier, NowMs());
  testing::Mock::VerifyAndClearExpectations(&cluster_handler_);

  // Verify that repeated probes aren't sent.
  EXPECT_CALL(cluster_handler_, OnProbeClusterConfig(_)).Times(0);
  probe_controller_->SetEstimatedBitrate(100 * kMbpsMultiplier, NowMs());
}

}  // namespace test
}  // namespace webrtc_cc
}  // namespace webrtc
