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
#include "p2p/base/teststunserver.h"
#include "p2p/base/testturnserver.h"
#include "p2p/client/basicportallocator.h"
#include "rtc_base/fakenetwork.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtualsocketserver.h"

using cricket::Candidate;
using cricket::PortAllocator;
using cricket::PortAllocatorSession;
using cricket::PortInterface;
using cricket::IceRegatheringReason;
using rtc::Thread;
using rtc::SocketAddress;

namespace {

const int kAllPorts = 0;
const int kOnlyLocalPorts = cricket::PORTALLOCATOR_DISABLE_STUN |
                            cricket::PORTALLOCATOR_DISABLE_RELAY |
                            cricket::PORTALLOCATOR_DISABLE_TCP;

const SocketAddress kLocalAddr("11.11.11.11", 0);
// The address of the public STUN server.
const SocketAddress kStunAddr("99.99.99.1", cricket::STUN_SERVER_PORT);
// The addresses for the public TURN server.
const SocketAddress kTurnUdpIntAddr("99.99.99.3", 3478);
const SocketAddress kTurnUdpExtAddr("99.99.99.6", 0);

const cricket::RelayCredentials kRelayCredentials("test", "test");
const char kIceUfrag[] = "UF00";
const char kIcePwd[] = "TESTICEPWD00000000000000";

// Return true if the addresses are the same, or the port is 0 in |pattern|
// (acting as a wildcard) and the IPs are the same.
// Even with a wildcard port, the port of the address should be nonzero if
// the IP is nonzero.
static bool AddressMatch(const SocketAddress& address,
                         const SocketAddress& pattern) {
  return address.ipaddr() == pattern.ipaddr() &&
         ((pattern.port() == 0 &&
           (address.port() != 0 || IPIsAny(address.ipaddr()))) ||
          (pattern.port() != 0 && address.port() == pattern.port()));
}

static bool FindCandidate(const std::vector<Candidate>& candidates,
                          const std::string& type,
                          const std::string& proto,
                          const SocketAddress& addr,
                          Candidate* found) {
  auto it = std::find_if(candidates.begin(), candidates.end(),
                         [type, proto, addr](const Candidate& c) {
                           return c.type() == type && c.protocol() == proto &&
                                  AddressMatch(c.address(), addr);
                         });
  if (it != candidates.end() && found) {
    *found = *it;
  }
  return it != candidates.end();
}

static bool HasCandidate(const std::vector<Candidate>& candidates,
                         const std::string& type,
                         const std::string& proto,
                         const SocketAddress& addr) {
  return FindCandidate(candidates, type, proto, addr, nullptr);
}

}  // namespace

namespace webrtc {

class RegatheringControllerTest : public testing::Test,
                                  public sigslot::has_slots<> {
 public:
  // The default config provides a simple setup. Tests can change the config and
  // provide it to the initialization methods below to construct a more complex
  // scenario, e.g. using a BasicPortAllocator for the candidate gathering
  // instead of a FakePortAllocator.
  class Config {
   public:
    Config()
        : allocator(
              new cricket::FakePortAllocator(Thread::Current(), nullptr)) {
      stun_servers.insert(kStunAddr);
      cricket::RelayServerConfig turn_server(cricket::RELAY_TURN);
      turn_server.credentials = kRelayCredentials;
      turn_server.ports.push_back(
          cricket::ProtocolAddress(kTurnUdpIntAddr, cricket::PROTO_UDP));
      turn_servers.push_back(turn_server);
    }
    PortAllocator* allocator;
    int allocator_flag = kOnlyLocalPorts;
    int candidate_filter = cricket::CF_ALL;
    cricket::ServerAddresses stun_servers;
    std::vector<cricket::RelayServerConfig> turn_servers;
  };

  RegatheringControllerTest()
      : ice_transport_(new cricket::MockIceTransport()),
        vss_(new rtc::VirtualSocketServer()),
        thread_(vss_.get()),
        stun_server_(
            cricket::TestStunServer::Create(Thread::Current(), kStunAddr)),
        turn_server_(Thread::Current(), kTurnUdpIntAddr, kTurnUdpExtAddr) {
    BasicRegatheringController::Config regathering_config(rtc::nullopt, 0);
    regathering_controller_.reset(new BasicRegatheringController(
        regathering_config, ice_transport_.get(), Thread::Current()));
  }

  // Initializes the allocator and gathers candidates once by StartGettingPorts.
  void InitializeAndGatherOnce(const Config& config) {
    allocator_ = rtc::WrapUnique(config.allocator);
    allocator_->set_flags(config.allocator_flag);
    allocator_->SetConfiguration(config.stun_servers, config.turn_servers,
                                 0 /* pool size */,
                                 false /* prune turn ports */);
    allocator_->Initialize();
    allocator_session_ = allocator_->CreateSession(
        "test", cricket::ICE_CANDIDATE_COMPONENT_RTP, kIceUfrag, kIcePwd);
    // The gathering will take place on the current thread and the following
    // call of StartGettingPorts is blocking. We will not ClearGettingPorts
    // prematurely.
    allocator_session_->SignalIceRegathering.connect(
        this, &RegatheringControllerTest::OnIceRegathering);
    allocator_session_->SignalCandidatesReady.connect(
        this, &RegatheringControllerTest::OnCandidatesReady);
    allocator_session_->SignalCandidatesRemoved.connect(
        this, &RegatheringControllerTest::OnCandidatesRemoved);
    allocator_session_->SignalCandidatesAllocationDone.connect(
        this, &RegatheringControllerTest::OnCandidatesAllocationDone);
    allocator_session_->SignalPortReady.connect(
        this, &RegatheringControllerTest::OnPortReady);
    allocator_session_->SetCandidateFilter(config.candidate_filter);
    allocator_session_->StartGettingPorts();
    regathering_controller_->SetAllocatorSession(allocator_session_.get());
  }

  // The regathering controller is initialized with the allocator session
  // cleared. Only after clearing the session, we would be able to regather. See
  // the comments for BasicRegatheringController in regatheringcontroller.h.
  void InitializeAndGatherOnceWithSessionCleared(const Config& config) {
    InitializeAndGatherOnce(config);
    allocator_session_->ClearGettingPorts();
  }

  void OnIceRegathering(PortAllocatorSession* allocator_session,
                        IceRegatheringReason reason) {
    ++count_[reason];
    std::string reason_text;
    switch (reason) {
      case IceRegatheringReason::OCCASIONAL_REFRESH:
        reason_text = "ocassional refresh";
        break;
      case IceRegatheringReason::NETWORK_CHANGE:
        reason_text = "network changed";
        break;
      case IceRegatheringReason::NETWORK_FAILURE:
        reason_text = "network failure";
        break;
      default:
        RTC_NOTREACHED();
    }
    RTC_LOG(INFO) << "Start ICE regathering due to " << reason_text;
    candidate_allocation_done_ = false;
  }

  void OnCandidatesReady(PortAllocatorSession* allocator_session,
                         const std::vector<Candidate>& candidates) {
    candidates_.insert(candidates_.end(), candidates.begin(), candidates.end());
  }

  void OnCandidatesRemoved(PortAllocatorSession* session,
                           const std::vector<Candidate>& removed_candidates) {
    auto new_end = std::remove_if(
        candidates_.begin(), candidates_.end(),
        [removed_candidates](Candidate& candidate) {
          for (const Candidate& removed_candidate : removed_candidates) {
            if (candidate.MatchesForRemoval(removed_candidate)) {
              return true;
            }
          }
          return false;
        });
    candidates_.erase(new_end, candidates_.end());
  }

  void OnCandidatesAllocationDone(PortAllocatorSession* allocator_session) {
    candidate_allocation_done_ = true;
  }

  void OnPortReady(PortAllocatorSession* allocator_session,
                   PortInterface* port) {
    ports_.push_back(port);
  }

  int GetRegatheringReasonCount(IceRegatheringReason reason) {
    return count_[reason];
  }

  BasicRegatheringController* regathering_controller() {
    return regathering_controller_.get();
  }

 protected:
  std::unique_ptr<cricket::IceTransportInternal> ice_transport_;
  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread thread_;
  std::unique_ptr<cricket::TestStunServer> stun_server_;
  cricket::TestTurnServer turn_server_;
  bool candidate_allocation_done_ = false;
  std::vector<Candidate> candidates_;
  std::vector<PortInterface*> ports_;
  std::unique_ptr<BasicRegatheringController> regathering_controller_;
  rtc::FakeNetworkManager network_manager_;
  std::unique_ptr<PortAllocator> allocator_;
  std::unique_ptr<PortAllocatorSession> allocator_session_;
  std::map<IceRegatheringReason, int> count_;
};

// Tests that ICE regathering occurs only if the port allocator session is
// cleared. A port allocation session is not cleared if the initial gathering is
// still in progress or the continual gathering is not enabled.
TEST_F(RegatheringControllerTest,
       IceRegatheringDoesNotOccurIfSessionNotCleared) {
  rtc::ScopedFakeClock clock;
  RegatheringControllerTest::Config default_test_config;
  InitializeAndGatherOnce(default_test_config);  // Session not cleared.

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
  EXPECT_EQ(
      0, GetRegatheringReasonCount(IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0,
            GetRegatheringReasonCount(IceRegatheringReason::NETWORK_FAILURE));
}

// Tests that ICE regathering on all networks can be canceled by changing the
// config.
TEST_F(RegatheringControllerTest, IceRegatheringOnAllNetworksCanBeCanceled) {
  rtc::ScopedFakeClock clock;
  RegatheringControllerTest::Config default_test_config;
  InitializeAndGatherOnceWithSessionCleared(default_test_config);

  const int kRegatherInterval = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(kRegatherInterval,
                                                          kRegatherInterval);
  BasicRegatheringController::Config config(
      regather_all_networks_interval_range, kRegatherInterval);
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  SIMULATED_WAIT(false, kRegatherInterval - 1, clock);
  // Expect no regathering.
  EXPECT_EQ(
      0, GetRegatheringReasonCount(IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0,
            GetRegatheringReasonCount(IceRegatheringReason::NETWORK_FAILURE));
  SIMULATED_WAIT(false, 2, clock);
  // Expect regathering on all networks and on failed networks to happen once
  // respectively in that last 2s with 2s interval.
  EXPECT_EQ(
      1, GetRegatheringReasonCount(IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(1,
            GetRegatheringReasonCount(IceRegatheringReason::NETWORK_FAILURE));
  const int kRegatherIntervalLong = 20000;
  config.regather_on_all_networks_interval_range =
      rtc::IntervalRange(kRegatherIntervalLong, kRegatherIntervalLong);
  // Set the config of regathering interval range on all networks should cancel
  // and reschedule the regathering on all networks.
  regathering_controller()->SetConfig(config);
  const int kNetworkGatherDurationLong = 10000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect no regathering on all networks happened in the last 10s.
  EXPECT_EQ(
      1, GetRegatheringReasonCount(IceRegatheringReason::OCCASIONAL_REFRESH));
}

// Tests that canceling the regathering on all networks does not cancel the
// schedule on failed networks.
TEST_F(RegatheringControllerTest,
       CancelingRegatheringOnAllNetworksDoesNotCancelOnFailedNetworks) {
  rtc::ScopedFakeClock clock;
  RegatheringControllerTest::Config default_test_config;
  InitializeAndGatherOnceWithSessionCleared(default_test_config);

  const int kRegatherIntervalShort = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(
      kRegatherIntervalShort, kRegatherIntervalShort);
  BasicRegatheringController::Config config(
      regather_all_networks_interval_range, kRegatherIntervalShort);
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  SIMULATED_WAIT(false, 1000, clock);
  // Expect no regathering.
  EXPECT_EQ(
      0, GetRegatheringReasonCount(IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0,
            GetRegatheringReasonCount(IceRegatheringReason::NETWORK_FAILURE));
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
  EXPECT_EQ(5,
            GetRegatheringReasonCount(IceRegatheringReason::NETWORK_FAILURE));
}

// Tests that ICE regathering on failed networks can be canceled by changing the
// config.
TEST_F(RegatheringControllerTest, IceRegatheringOnFailedNetworksCanBeCanceled) {
  rtc::ScopedFakeClock clock;
  RegatheringControllerTest::Config default_test_config;
  InitializeAndGatherOnceWithSessionCleared(default_test_config);

  const int kRegatherInterval = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(kRegatherInterval,
                                                          kRegatherInterval);
  BasicRegatheringController::Config config(
      regather_all_networks_interval_range, kRegatherInterval);
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  SIMULATED_WAIT(false, 1000, clock);
  // Expect no regathering.
  EXPECT_EQ(
      0, GetRegatheringReasonCount(IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0,
            GetRegatheringReasonCount(IceRegatheringReason::NETWORK_FAILURE));
  SIMULATED_WAIT(false, kRegatherInterval + 1, clock);
  // Expect regathering on all networks and on failed networks to happen once
  // respectively in 3s with 2s interval.
  EXPECT_EQ(
      1, GetRegatheringReasonCount(IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(1,
            GetRegatheringReasonCount(IceRegatheringReason::NETWORK_FAILURE));
  const int kRegatherIntervalLong = 20000;
  config.regather_on_failed_networks_interval = kRegatherIntervalLong;
  // Set the config of regathering interval on failed networks should cancel
  // and reschedule the regathering on failed networks.
  regathering_controller()->SetConfig(config);
  const int kNetworkGatherDurationLong = 10000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect no regathering on failed networks happened in the last 10s.
  EXPECT_EQ(1,
            GetRegatheringReasonCount(IceRegatheringReason::NETWORK_FAILURE));
}

// Tests that canceling the regathering on failed networks does not cancel the
// schedule on all networks.
TEST_F(RegatheringControllerTest,
       CancelingRegatheringOnFailedNetworksDoesNotCancelOnAllNetworks) {
  rtc::ScopedFakeClock clock;
  RegatheringControllerTest::Config default_test_config;
  InitializeAndGatherOnceWithSessionCleared(default_test_config);

  const int kRegatherIntervalShort = 2000;
  rtc::IntervalRange regather_all_networks_interval_range(
      kRegatherIntervalShort, kRegatherIntervalShort);
  BasicRegatheringController::Config config(
      regather_all_networks_interval_range, kRegatherIntervalShort);
  regathering_controller()->SetConfig(config);
  regathering_controller()->Start();
  SIMULATED_WAIT(false, 1000, clock);
  EXPECT_EQ(
      0, GetRegatheringReasonCount(IceRegatheringReason::OCCASIONAL_REFRESH));
  EXPECT_EQ(0,
            GetRegatheringReasonCount(IceRegatheringReason::NETWORK_FAILURE));
  const int kRegatherIntervalLong = 20000;
  config.regather_on_failed_networks_interval = kRegatherIntervalLong;
  regathering_controller()->SetConfig(config);
  const int kNetworkGatherDurationLong = 10000;
  SIMULATED_WAIT(false, kNetworkGatherDurationLong, clock);
  // Expect regathering to happen for 5 times for all networks in the last 11s
  // with 2s interval.
  EXPECT_EQ(
      5, GetRegatheringReasonCount(IceRegatheringReason::OCCASIONAL_REFRESH));
}

// Tests that the schedule of ICE regathering on all networks can be canceled
// and replaced by a new recurring schedule.
TEST_F(RegatheringControllerTest,
       ScheduleOfIceRegatheringOnAllNetworkCanBeReplaced) {
  rtc::ScopedFakeClock clock;
  RegatheringControllerTest::Config default_test_config;
  InitializeAndGatherOnceWithSessionCleared(default_test_config);

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
  EXPECT_EQ(
      1, GetRegatheringReasonCount(IceRegatheringReason::OCCASIONAL_REFRESH));
  const int kRegatherIntervalLong = 5000;
  config.regather_on_all_networks_interval_range =
      rtc::IntervalRange(kRegatherIntervalLong, kRegatherIntervalLong);
  regathering_controller()->SetConfig(config);
  const int kNetworkGatherDurationLong = 11000;
  SIMULATED_WAIT(false, 1500, clock);
  // Expect no regathering from the previous schedule.
  EXPECT_EQ(
      1, GetRegatheringReasonCount(IceRegatheringReason::OCCASIONAL_REFRESH));
  SIMULATED_WAIT(false, kNetworkGatherDurationLong - 1500, clock);
  // Expect regathering to happen twice in the last 11s with 5s interval.
  EXPECT_EQ(
      3, GetRegatheringReasonCount(IceRegatheringReason::OCCASIONAL_REFRESH));
}

// Tests that the schedule of ICE regathering on failed networks can be canceled
// and replaced by a new recurring schedule.
TEST_F(RegatheringControllerTest,
       ScheduleOfIceRegatheringOnFailedNetworkCanBeReplaced) {
  rtc::ScopedFakeClock clock;
  RegatheringControllerTest::Config default_test_config;
  InitializeAndGatherOnceWithSessionCleared(default_test_config);

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
  EXPECT_EQ(1,
            GetRegatheringReasonCount(IceRegatheringReason::NETWORK_FAILURE));
  const int kRegatherIntervalLong = 5000;
  config.regather_on_failed_networks_interval = kRegatherIntervalLong;
  regathering_controller()->SetConfig(config);
  const int kNetworkGatherDurationLong = 11000;
  SIMULATED_WAIT(false, 1500, clock);
  // Expect no regathering from the previous schedule.
  EXPECT_EQ(1,
            GetRegatheringReasonCount(IceRegatheringReason::NETWORK_FAILURE));
  SIMULATED_WAIT(false, kNetworkGatherDurationLong - 1500, clock);
  // Expect regathering to happen twice in the last 11s with 5s interval.
  EXPECT_EQ(3,
            GetRegatheringReasonCount(IceRegatheringReason::NETWORK_FAILURE));
}

TEST_F(RegatheringControllerTest,
       ChangingCandidateFilterFromRelayOnlyToAllGathersOtherCanddiates) {
  rtc::ScopedFakeClock clock;
  network_manager_.AddInterface(kLocalAddr);
  RegatheringControllerTest::Config config;
  // We will use a different config to be able to gather all types of
  // candidates.
  config.allocator = new cricket::BasicPortAllocator(&network_manager_);
  config.allocator_flag = kAllPorts;
  config.candidate_filter = cricket::CF_RELAY;
  InitializeAndGatherOnceWithSessionCleared(config);
  EXPECT_TRUE_SIMULATED_WAIT(candidate_allocation_done_, 10000, clock);
  EXPECT_EQ(1u, candidates_.size());
  EXPECT_TRUE(HasCandidate(candidates_, "relay", "udp",
                           SocketAddress(kTurnUdpExtAddr.ipaddr(), 0)));
  RTC_LOG(INFO) << "Change the candidate filter";
  allocator_session_->SetCandidateFilter(cricket::CF_ALL);
  // Wait a bit for resetting candidate_allocation_done_ after
  // SignalCandidateFilterChanged.
  SIMULATED_WAIT(false, 1000, clock)
  EXPECT_TRUE_SIMULATED_WAIT(candidate_allocation_done_, 10000, clock);
  EXPECT_EQ(4u, candidates_.size());
  EXPECT_TRUE(HasCandidate(candidates_, "local", "udp",
                           SocketAddress(kLocalAddr.ipaddr(), 0)));
  // In our setup, the endpoint is not behind a NAT, so the STUN candidate
  // should have the lcoal address.
  EXPECT_TRUE(HasCandidate(candidates_, "stun", "udp",
                           SocketAddress(kLocalAddr.ipaddr(), 0)));
  EXPECT_TRUE(HasCandidate(candidates_, "relay", "udp",
                           SocketAddress(kTurnUdpExtAddr.ipaddr(), 0)));
  EXPECT_TRUE(HasCandidate(candidates_, "local", "tcp",
                           SocketAddress(kLocalAddr.ipaddr(), 0)));
}

}  // namespace webrtc
