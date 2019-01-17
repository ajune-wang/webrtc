#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "p2p/base/sim_core.h"

#include "absl/memory/memory.h"
#include "p2p/base/port.h"
#include "p2p/base/stun_server.h"
#include "p2p/client/basicportallocator.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/gunit.h"
#include "rtc_base/nethelpers.h"
#include "rtc_base/network.h"
#include "rtc_base/physicalsocketserver.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"

namespace {

const webrtc::SimInterfaceConfig iface_config1{
    "tun1", "10.0.0.1", "255.255.255.0", rtc::ADAPTER_TYPE_CELLULAR,
    webrtc::SimInterface::State::kUp};
const webrtc::SimInterfaceConfig iface_config2{
    "tun2", "172.16.0.1", "255.255.255.0", rtc::ADAPTER_TYPE_WIFI,
    webrtc::SimInterface::State::kUp};

}  // namespace

namespace webrtc {

const std::string kSimInterfaceName = "tun1";
const rtc::SocketAddress kSimNetworkIp("10.0.0.1", 0);
const int kGatherLocalAndStun =
    cricket::PORTALLOCATOR_DISABLE_RELAY | cricket::PORTALLOCATOR_DISABLE_TCP;

class SimIceGatheringTest : public testing::Test, public sigslot::has_slots<> {
 public:
  SimIceGatheringTest() : core_(absl::make_unique<SimCore>()) {
    SimConfig config;
    config.webrtc_network_thread = rtc::Thread::Current();
    config.iface_configs.emplace_back(iface_config1);
    config.iface_configs.emplace_back(iface_config2);
    core_->Init(config);
    invoker_.AsyncInvoke<bool>(RTC_FROM_HERE, core_->nio_thread(),
                               rtc::Bind(&SimCore::Start, core_.get()));

    cricket::ServerAddresses stun_servers;
    stun_servers.insert(
        rtc::SocketAddress("99.99.99.254", cricket::STUN_SERVER_PORT));
    network_manager_ = core_->CreateNetworkManager({"tun1", "tun2"});
    allocator_ =
        absl::make_unique<cricket::BasicPortAllocator>(network_manager_.get());
    allocator_->set_flags(kGatherLocalAndStun);
    allocator_->set_step_delay(cricket::kMinimumStepDelay);
    allocator_->SetConfiguration(stun_servers,
                                 std::vector<cricket::RelayServerConfig>(), 0,
                                 false, nullptr);
    allocator_->Initialize();

    session_ = allocator_->CreateSession("net_sim", 0, "", "");
    session_->SignalPortReady.connect(this, &SimIceGatheringTest::OnPortReady);
    session_->SignalCandidatesReady.connect(
        this, &SimIceGatheringTest::OnCandidatesReady);
    session_->SignalCandidatesAllocationDone.connect(
        this, &SimIceGatheringTest::OnCandidatesAllocationDone);
  }

  void OnPortReady(cricket::PortAllocatorSession* session,
                   cricket::PortInterface* port) {
    RTC_LOG(INFO) << "OnPortReady: " << port->ToString();
    ports_.push_back(port);
  }

  void OnCandidatesReady(cricket::PortAllocatorSession* session,
                         const std::vector<cricket::Candidate>& candidates) {
    for (const cricket::Candidate& c : candidates) {
      RTC_LOG(INFO) << "OnCandidatesReady: " << c.ToString();
      candidates_.push_back(c);
      if (c.type() == cricket::LOCAL_PORT_TYPE) {
        SimInterface* iface = core_->GetInterface(c.address().ipaddr());
        RTC_DCHECK(iface != nullptr);
        core_->CreateAndBindSocketOnDualInterface(iface->dual(),
                                                  c.address().port());
      }
    }
  }

  void OnCandidatesAllocationDone(cricket::PortAllocatorSession* session) {
    ASSERT_FALSE(candidates_allocation_done_);
    candidates_allocation_done_ = true;
  }

  bool HasCandidate(const rtc::SocketAddress& address,
                    const std::string& candidate_type,
                    const std::string& network_name,
                    rtc::AdapterType network_type) {
    for (const auto& c : candidates_) {
      if (c.address().ipaddr() == address.ipaddr() &&
          c.type() == candidate_type && c.network_name() == network_name &&
          c.network_type() == network_type) {
        return true;
      }
    }
    return false;
  }

 protected:
  rtc::AsyncInvoker invoker_;
  std::unique_ptr<SimCore> core_;
  std::unique_ptr<SimNetworkManager> network_manager_;
  std::unique_ptr<cricket::PortAllocatorSession> session_;
  std::unique_ptr<cricket::BasicPortAllocator> allocator_;
  std::vector<cricket::PortInterface*> ports_;
  std::vector<cricket::Candidate> candidates_;
  bool candidates_allocation_done_ = false;
};

TEST_F(SimIceGatheringTest, TestBasics) {
  EXPECT_TRUE_WAIT(core_->started(), 1000);
  session_->StartGettingPorts();
  EXPECT_TRUE_WAIT(candidates_allocation_done_, 1000);
  ASSERT_EQ(4u, ports_.size());
  ASSERT_EQ(4u, candidates_.size());
  EXPECT_TRUE(HasCandidate(rtc::SocketAddress("10.0.0.1", 0),
                           cricket::LOCAL_PORT_TYPE, "tun1",
                           rtc::ADAPTER_TYPE_CELLULAR));
  EXPECT_TRUE(HasCandidate(rtc::SocketAddress("10.0.0.254", 0),
                           cricket::STUN_PORT_TYPE, "tun1",
                           rtc::ADAPTER_TYPE_CELLULAR));
  EXPECT_TRUE(HasCandidate(rtc::SocketAddress("172.16.0.1", 0),
                           cricket::LOCAL_PORT_TYPE, "tun2",
                           rtc::ADAPTER_TYPE_WIFI));
  EXPECT_TRUE(HasCandidate(rtc::SocketAddress("172.16.0.254", 0),
                           cricket::STUN_PORT_TYPE, "tun2",
                           rtc::ADAPTER_TYPE_WIFI));
}

}  // namespace webrtc
