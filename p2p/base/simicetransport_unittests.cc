#include "absl/memory/memory.h"
#include "p2p/base/p2ptransportchannel.h"
#include "p2p/base/sim_core.h"
#include "p2p/client/basicportallocator.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/gunit.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace {

const char* kIceUfrag[4] = {"UF00", "UF01", "UF02", "UF03"};
const char* kIcePwd[4] = {
    "TESTICEPWD00000000000000", "TESTICEPWD00000000000001",
    "TESTICEPWD00000000000002", "TESTICEPWD00000000000003"};
const cricket::IceParameters kIceParams[4] = {
    {kIceUfrag[0], kIcePwd[0], false},
    {kIceUfrag[1], kIcePwd[1], false},
    {kIceUfrag[2], kIcePwd[2], false},
    {kIceUfrag[3], kIcePwd[3], false}};

// const int kOnlyLocalPorts = cricket::PORTALLOCATOR_DISABLE_STUN |
//                             cricket::PORTALLOCATOR_DISABLE_RELAY |
//                             cricket::PORTALLOCATOR_DISABLE_TCP;

const int kLocalAndStunPorts =
    cricket::PORTALLOCATOR_DISABLE_RELAY | cricket::PORTALLOCATOR_DISABLE_TCP;

const webrtc::SimInterfaceConfig iface_config1{
    "tun1", "10.0.0.1", "255.255.255.0", rtc::ADAPTER_TYPE_CELLULAR,
    webrtc::SimInterface::State::kUp};
const webrtc::SimInterfaceConfig iface_config2{
    "tun2", "172.16.0.1", "255.255.255.0", rtc::ADAPTER_TYPE_WIFI,
    webrtc::SimInterface::State::kDown};
const webrtc::SimInterfaceConfig iface_config3{
    "tun3", "192.168.0.1", "255.255.255.0", rtc::ADAPTER_TYPE_WIFI,
    webrtc::SimInterface::State::kUp};

const webrtc::SimLinkConfig link_config1{"bp2p_link1",
                                         webrtc::SimLink::Type::kPointToPoint,
                                         {"10.0.0.1", "192.168.0.1"},
                                         webrtc::SimLinkConfig::Params()};
// TODO(qingsi): manually manage the config is too error-prone.
const webrtc::SimLinkConfig link_config2{"bp2p_link1",
                                         webrtc::SimLink::Type::kPointToPoint,
                                         {"172.16.0.1", "192.168.0.1"},
                                         webrtc::SimLinkConfig::Params()};
}  // namespace

namespace webrtc {

using namespace cricket;

class SimIceTransportTest : public testing::Test, public sigslot::has_slots<> {
 public:
  SimIceTransportTest() : core_(absl::make_unique<SimCore>()) {
    SimConfig config;
    config.webrtc_network_thread = rtc::Thread::Current();
    config.iface_configs.emplace_back(iface_config1);
    config.iface_configs.emplace_back(iface_config2);
    config.iface_configs.emplace_back(iface_config3);
    config.link_configs.emplace_back(link_config1);
    config.link_configs.emplace_back(link_config2);
    core_->Init(config);
    invoker_.AsyncInvoke<bool>(RTC_FROM_HERE, core_->nio_thread(),
                               rtc::Bind(&SimCore::Start, core_.get()));

    cricket::ServerAddresses stun_servers;
    stun_servers.insert(
        rtc::SocketAddress("99.99.99.254", cricket::STUN_SERVER_PORT));

    // "tun2" starts with state kDown.
    ep1_.role_ = ICEROLE_CONTROLLING;
    ep1_.network_manager_ = core_->CreateNetworkManager({"tun1", "tun2"});
    ep1_.allocator_ = absl::make_unique<cricket::BasicPortAllocator>(
        ep1_.network_manager_.get());
    ep1_.allocator_->set_step_delay(kMinimumStepDelay);
    ep1_.allocator_->set_flags(kLocalAndStunPorts |
                               PORTALLOCATOR_ENABLE_SHARED_SOCKET);
    ep1_.allocator_->SetConfiguration(stun_servers,
                                      std::vector<cricket::RelayServerConfig>(),
                                      0, false, nullptr);
    ep1_.allocator_->Initialize();

    ep2_.role_ = ICEROLE_CONTROLLED;
    ep2_.network_manager_ = core_->CreateNetworkManager({"tun3"});
    ep2_.allocator_ = absl::make_unique<cricket::BasicPortAllocator>(
        ep2_.network_manager_.get());
    ep2_.allocator_->set_step_delay(kMinimumStepDelay);
    ep2_.allocator_->set_flags(kLocalAndStunPorts |
                               PORTALLOCATOR_ENABLE_SHARED_SOCKET);
    ep2_.allocator_->SetConfiguration(stun_servers,
                                      std::vector<cricket::RelayServerConfig>(),
                                      0, false, nullptr);
    ep2_.allocator_->Initialize();
  }

 protected:
  struct Endpoint : public sigslot::has_slots<> {
    Endpoint() : role_(ICEROLE_UNKNOWN) {}

    std::unique_ptr<SimNetworkManager> network_manager_;
    std::unique_ptr<BasicPortAllocator> allocator_;
    std::unique_ptr<P2PTransportChannel> ch_;
    IceRole role_;
  };

  Endpoint* GetEndpoint(int endpoint) {
    if (endpoint == 0) {
      return &ep1_;
    } else if (endpoint == 1) {
      return &ep2_;
    }
    RTC_NOTREACHED();
    return nullptr;
  }

  void CreateChannels(const IceConfig& ep1_config,
                      const IceConfig& ep2_config) {
    ep1_.ch_.reset(CreateChannel(0, ICE_CANDIDATE_COMPONENT_DEFAULT,
                                 kIceParams[0], kIceParams[1]));
    ep2_.ch_.reset(CreateChannel(1, ICE_CANDIDATE_COMPONENT_DEFAULT,
                                 kIceParams[1], kIceParams[0]));
    ep1_.ch_->SetIceConfig(ep1_config);
    ep2_.ch_->SetIceConfig(ep2_config);
    ep1_.ch_->MaybeStartGathering();
    ep2_.ch_->MaybeStartGathering();
  }

  void CreateChannels() {
    IceConfig default_config;
    default_config.continual_gathering_policy = GATHER_CONTINUALLY;
    CreateChannels(default_config, default_config);
  }

  P2PTransportChannel* CreateChannel(int endpoint,
                                     int component,
                                     const IceParameters& local_ice,
                                     const IceParameters& remote_ice) {
    P2PTransportChannel* channel = new P2PTransportChannel(
        "sim_content", component, GetEndpoint(endpoint)->allocator_.get());
    channel->SignalCandidateGathered.connect(
        this, &SimIceTransportTest::OnCandidateGathered);
    channel->SignalCandidatesRemoved.connect(
        this, &SimIceTransportTest::OnCandidatesRemoved);
    channel->SignalRoleConflict.connect(this,
                                        &SimIceTransportTest::OnRoleConflict);
    channel->SignalNetworkRouteChanged.connect(
        this, &SimIceTransportTest::OnNetworkRouteChanged);
    channel->SetIceParameters(local_ice);
    channel->SetRemoteIceParameters(remote_ice);
    channel->SetIceRole(GetEndpoint(endpoint)->role_);
    return channel;
  }
  void DestroyChannels() {
    ep1_.ch_.reset();
    ep2_.ch_.reset();
  }
  Endpoint* ep1() { return &ep1_; }
  Endpoint* ep2() { return &ep2_; }
  P2PTransportChannel* ep1_ch() { return ep1_.ch_.get(); }
  P2PTransportChannel* ep2_ch() { return ep2_.ch_.get(); }

  // We pass the candidates directly to the other side.
  void OnCandidateGathered(IceTransportInternal* ch, const Candidate& c) {
    P2PTransportChannel* remote_ch;
    if (ch->GetIceRole() == ICEROLE_CONTROLLING) {
      remote_ch = ep2_.ch_.get();
    } else if (ch->GetIceRole() == ICEROLE_CONTROLLED) {
      remote_ch = ep1_.ch_.get();
    } else {
      RTC_NOTREACHED();
    }
    if (c.type() == cricket::LOCAL_PORT_TYPE) {
      SimInterface* iface = core_->GetInterface(c.address().ipaddr());
      RTC_DCHECK(iface != nullptr);
      core_->CreateAndBindSocketOnDualInterface(iface->dual(),
                                                c.address().port());
      return;
    }
    RTC_LOG(INFO) << "Signaling candidate " << c.ToString();
    remote_ch->AddRemoteCandidate(c);
  }

  void OnNetworkRouteChanged(absl::optional<rtc::NetworkRoute> network_route) {
    if (network_route) {
      RTC_LOG(INFO) << "Network route changed.";
    }
  }

  void OnCandidatesRemoved(IceTransportInternal* ch,
                           const std::vector<Candidate>& candidates) {
    RTC_NOTREACHED();
  }

  void OnRoleConflict(IceTransportInternal* channel) { RTC_NOTREACHED(); }

 protected:
  rtc::AsyncInvoker invoker_;
  std::unique_ptr<SimCore> core_;
  Endpoint ep1_;
  Endpoint ep2_;
};

TEST_F(SimIceTransportTest, TestBasics) {
  EXPECT_TRUE_WAIT(core_->started(), 1000);
  CreateChannels();
  ASSERT_TRUE_WAIT(ep1_ch()->writable() && ep1_ch()->selected_connection(),
                   1000);
  EXPECT_EQ("tun1",
            ep1_ch()->selected_connection()->local_candidate().network_name());
  ASSERT_TRUE_WAIT(ep2_ch()->writable() && ep2_ch()->selected_connection(),
                   1000);
  EXPECT_EQ("tun3",
            ep2_ch()->selected_connection()->local_candidate().network_name());

  auto iface = core_->GetInterface("tun2");
  ASSERT_NE(nullptr, iface);
  RTC_LOG(INFO) << "Bring up tun2";
  iface->SetState(SimInterface::State::kUp);
  ASSERT_TRUE_WAIT(
      ep1_ch()->selected_connection() &&
          ep1_ch()->selected_connection()->local_candidate().network_name() ==
              "tun2",
      1000);
  DestroyChannels();
}

}  // namespace webrtc
