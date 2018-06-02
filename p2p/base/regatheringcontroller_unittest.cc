/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/fakemetricsobserver.h"
#include "p2p/base/fakeportallocator.h"
#include "p2p/base/mockicetransport.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/port.h"
#include "p2p/base/regatheringcontroller.h"
#include "p2p/base/stunserver.h"
#include "rtc_base/gunit.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/thread.h"

namespace {

const int kOnlyLocalPorts = cricket::PORTALLOCATOR_DISABLE_STUN |
                            cricket::PORTALLOCATOR_DISABLE_RELAY |
                            cricket::PORTALLOCATOR_DISABLE_TCP;
// The address of the public STUN server.
const rtc::SocketAddress kStunAddr("99.99.99.1", cricket::STUN_SERVER_PORT);
// The addresses for the public TURN server.
const rtc::SocketAddress kTurnUdpIntAddr("99.99.99.3",
                                         cricket::STUN_SERVER_PORT);
const cricket::RelayCredentials kRelayCredentials("test", "test");
const char kIceUfrag[] = "UF00";
const char kIcePwd[] = "TESTICEPWD00000000000000";

}  // namespace

namespace webrtc {

class RegatheringControllerTest : public testing::Test,
                                  public sigslot::has_slots<> {
 public:
  RegatheringControllerTest()
      : ice_transport_(new cricket::MockIceTransport()),
        allocator_(
            new cricket::FakePortAllocator(rtc::Thread::Current(), nullptr)) {
    BasicRegatheringController::Config regathering_config(rtc::nullopt, 0);
    regathering_controller_.reset(new BasicRegatheringController(
        regathering_config, ice_transport_.get(), rtc::Thread::Current()));
  }

  // Initializes the allocator and gathers candidates once by StartGettingPorts.
  void InitializeAndGatherOnce() {
    cricket::ServerAddresses stun_servers;
    stun_servers.insert(kStunAddr);
    cricket::RelayServerConfig turn_server(cricket::RELAY_TURN);
    turn_server.credentials = kRelayCredentials;
    turn_server.ports.push_back(
        cricket::ProtocolAddress(kTurnUdpIntAddr, cricket::PROTO_UDP));
    std::vector<cricket::RelayServerConfig> turn_servers(1, turn_server);
    allocator_->set_flags(kOnlyLocalPorts);
    allocator_->SetConfiguration(stun_servers, turn_servers, 0 /* pool size */,
                                 false /* prune turn ports */);
    allocator_session_ = allocator_->CreateSession(
        "test", cricket::ICE_CANDIDATE_COMPONENT_RTP, kIceUfrag, kIcePwd);
    // The gathering will take place on the current thread and the following
    // call of StartGettingPorts is blocking. We will not ClearGettingPorts
    // prematurely.
    allocator_session_->StartGettingPorts();
    allocator_session_->SignalIceRegathering.connect(
        this, &RegatheringControllerTest::OnIceRegathering);
    regathering_controller_->set_allocator_session(allocator_session_.get());
  }

  // The regathering controller is initialized with the allocator session
  // cleared. Only after clearing the session, we would be able to regather. See
  // the comments for BasicRegatheringController in regatheringcontroller.h.
  void InitializeAndGatherOnceWithSessionCleared() {
    InitializeAndGatherOnce();
    allocator_session_->ClearGettingPorts();
  }

  void OnIceRegathering(cricket::PortAllocatorSession* allocator_session,
                        cricket::IceRegatheringReason reason) {
    ++count_[reason];
  }

  int GetRegatheringReasonCount(cricket::IceRegatheringReason reason) {
    return count_[reason];
  }

  BasicRegatheringController* regathering_controller() {
    return regathering_controller_.get();
  }

 private:
  std::unique_ptr<cricket::IceTransportInternal> ice_transport_;
  std::unique_ptr<BasicRegatheringController> regathering_controller_;
  std::unique_ptr<cricket::PortAllocator> allocator_;
  std::unique_ptr<cricket::PortAllocatorSession> allocator_session_;
  std::map<cricket::IceRegatheringReason, int> count_;
};

// Tests that ICE regathering occurs only if the port allocator session is
// cleared. A port allocation session is not cleared if the initial gathering is
// still in progress or the continual gathering is not enabled.
TEST_F(RegatheringControllerTest,
       IceRegatheringDoesNotOccurIfSessionNotCleared) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnce();  // Session not cleared.

  const int kRegatherInterval = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(kRegatherInterval,
                                                          kRegatherInterval);
  BasicRegatheringController::Config config(
      regather_all_networks_interval_range, kRegatherInterval);
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  const int kNetworkGatherDurationLong = 10000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect no regathering in the last 10s.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
}

// Tests that ICE regathering on all networks can be canceled by changing the
// config.
TEST_F(RegatheringControllerTest, IceRegatheringOnAllNetworksCanBeCanceled) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnceWithSessionCleared();

  const int kRegatherInterval = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(kRegatherInterval,
                                                          kRegatherInterval);
  BasicRegatheringController::Config config(
      regather_all_networks_interval_range, kRegatherInterval);
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  SIMULATED_WAIT(false, kRegatherInterval - 1, clock);
  // Expect no regathering.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  SIMULATED_WAIT(false, 2, clock);
  // Expect regathering on all networks and on failed networks to happen once
  // respectively in that last 2s with 2s interval.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  const int kRegatherIntervalLong = 20000;
  config.regather_on_all_networks_interval_range =
      rtc::IntervalRange(kRegatherIntervalLong, kRegatherIntervalLong);
  // Set the config of regathering interval range on all networks should cancel
  // and reschedule the regathering on all networks.
  regathering_controller()->SetConfig(config);
  const int kNetworkGatherDurationLong = 10000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect no regathering on all networks happened in the last 10s.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
}

// Tests that canceling the regathering on all networks does not cancel the
// schedule on failed networks.
TEST_F(RegatheringControllerTest,
       CancelingRegatheringOnAllNetworksDoesNotCancelOnFailedNetworks) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnceWithSessionCleared();

  const int kRegatherIntervalShort = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(
      kRegatherIntervalShort, kRegatherIntervalShort);
  BasicRegatheringController::Config config(
      regather_all_networks_interval_range, kRegatherIntervalShort);
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  SIMULATED_WAIT(false, 1000, clock);
  // Expect no regathering.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  const int kRegatherIntervalLong = 20000;
  config.regather_on_all_networks_interval_range =
      rtc::IntervalRange(kRegatherIntervalLong, kRegatherIntervalLong);
  // Canceling and rescheduling the regathering on all networks should not
  // impact the schedule for failed networks.
  regathering_controller()->SetConfig(config);
  const int kNetworkGatherDurationLong = 10000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen for 5 times for failed networks in the last
  // 11s with 2s interval.
  EXPECT_EQ(5, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
}

// Tests that ICE regathering on failed networks can be canceled by changing the
// config.
TEST_F(RegatheringControllerTest, IceRegatheringOnFailedNetworksCanBeCanceled) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnceWithSessionCleared();

  const int kRegatherInterval = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(kRegatherInterval,
                                                          kRegatherInterval);
  BasicRegatheringController::Config config(
      regather_all_networks_interval_range, kRegatherInterval);
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  SIMULATED_WAIT(false, 1000, clock);
  // Expect no regathering.
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  SIMULATED_WAIT(false, kRegatherInterval + 1, clock);
  // Expect regathering on all networks and on failed networks to happen once
  // respectively in 3s with 2s interval.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  const int kRegatherIntervalLong = 20000;
  config.regather_on_failed_networks_interval = kRegatherIntervalLong;
  // Set the config of regathering interval on failed networks should cancel
  // and reschedule the regathering on failed networks.
  regathering_controller()->SetConfig(config);
  const int kNetworkGatherDurationLong = 10000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect no regathering on failed networks happened in the last 10s.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
}

// Tests that canceling the regathering on failed networks does not cancel the
// schedule on all networks.
TEST_F(RegatheringControllerTest,
       CancelingRegatheringOnFailedNetworksDoesNotCancelOnAllNetworks) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnceWithSessionCleared();

  const int kRegatherIntervalShort = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(
      kRegatherIntervalShort, kRegatherIntervalShort);
  BasicRegatheringController::Config config(
      regather_all_networks_interval_range, kRegatherIntervalShort);
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  SIMULATED_WAIT(false, 1000, clock);
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  const int kRegatherIntervalLong = 20000;
  config.regather_on_failed_networks_interval = kRegatherIntervalLong;
  regathering_controller()->SetConfig(config);
  const int kNetworkGatherDurationLong = 10000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen for 5 times for all networks in the last 11s
  // with 2s interval.
  EXPECT_EQ(5, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
}

// Tests that the schedule of ICE regathering on all networks can be canceled
// and replaced by a new recurring schedule.
TEST_F(RegatheringControllerTest,
       ScheduleOfIceRegatheringOnAllNetworkCanBeReplaced) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnceWithSessionCleared();

  const int kRegatherIntervalShort = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(
      kRegatherIntervalShort, kRegatherIntervalShort);
  BasicRegatheringController::Config config(
      regather_all_networks_interval_range, kRegatherIntervalShort);
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  const int kNetworkGatherDurationShort = 3000;
  SIMULATED_WAIT(false, kNetworkGatherDurationShort, clock);
  // Expect regathering happen to once on all networks in 3s.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  const int kRegatherIntervalLong = 5000;
  config.regather_on_all_networks_interval_range =
      rtc::IntervalRange(kRegatherIntervalLong, kRegatherIntervalLong);
  regathering_controller()->SetConfig(config);
  const int kNetworkGatherDurationLong = 11000;
  SIMULATED_WAIT(false, 1500, clock);
  // Expect no regathering from the previous schedule.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
  SIMULATED_WAIT(false, kNetworkGatherDurationLong - 1500, clock);
  // Expect regathering to happen twice in the last 11s with 5s interval.
  EXPECT_EQ(3, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::OCCASIONAL_REFRESH));
}

// Tests that the schedule of ICE regathering on failed networks can be canceled
// and replaced by a new recurring schedule.
TEST_F(RegatheringControllerTest,
       ScheduleOfIceRegatheringOnFailedNetworkCanBeReplaced) {
  rtc::ScopedFakeClock clock;
  InitializeAndGatherOnceWithSessionCleared();

  const int kRegatherIntervalShort = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(
      kRegatherIntervalShort, kRegatherIntervalShort);
  BasicRegatheringController::Config config(
      regather_all_networks_interval_range, kRegatherIntervalShort);
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  const int kNetworkGatherDurationShort = 3000;
  SIMULATED_WAIT(false, kNetworkGatherDurationShort, clock);
  // Expect regathering happen to once on all networks in 3s.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  const int kRegatherIntervalLong = 5000;
  config.regather_on_failed_networks_interval = kRegatherIntervalLong;
  regathering_controller()->SetConfig(config);
  const int kNetworkGatherDurationLong = 11000;
  SIMULATED_WAIT(false, 1500, clock);
  // Expect no regathering from the previous schedule.
  EXPECT_EQ(1, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
  SIMULATED_WAIT(false, kNetworkGatherDurationLong - 1500, clock);
  // Expect regathering to happen twice in the last 11s with 5s interval.
  EXPECT_EQ(3, GetRegatheringReasonCount(
                   cricket::IceRegatheringReason::NETWORK_FAILURE));
}

}  // namespace webrtc
