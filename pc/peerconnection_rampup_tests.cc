/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/stats/rtcstats_objects.h"
#include "api/test/fakeconstraints.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "p2p/base/testturnserver.h"
#include "p2p/client/basicportallocator.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/fakeperiodicvideotracksource.h"
#include "pc/test/fakevideotrackrenderer.h"
#include "rtc_base/fakenetwork.h"
#include "rtc_base/firewallsocketserver.h"
#include "rtc_base/gunit.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/virtualsocketserver.h"
#include "test/gtest.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {

namespace {
static const int kDefaultTimeout = 10000;
static const rtc::SocketAddress kDefaultLocalAddress("1.1.1.1", 0);

static const int64_t kPollIntervalMs = 50;
// When the pc factory creates a PeerConnection, it creates a call object with a
// starting bandwidth of 300 kbps. We would like to ramp up a little bit for our
// test, so the initial ramp up is for 500 kbps.
static const int kExpectedHighBitrateBps = 500000;
// The low bandwidth from rampup_tests.cc is 20 kbps, but since our virtual
// network's bandwidth is both for uplink & downlink we set it to 40 kbps.
static const int kLowBandwidthLimitBps = 40000;
static const int kLowBitrateMarginBps = 2000;
}  // namespace

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;

// This is an end to end test to verify that BWE is functioning when setting
// up a one to one call at the PeerConnection level. The intention of the test
// is to catch potential regressions for different ICE path configurations. The
// test uses a VirtualSocketServer for it's underlying simulated network and
// fake audio and video sources. The test is based upon rampup_tests.cc, but
// instead is at the PeerConnection level and uses a different fake network
// (rampup_tests.cc uses SimulatedNetwork). In the future, this test could
// potentially test different network conditions and test video quality as well
// (video_quality_test.cc does this, but at the call level).
class PeerConnectionWrapperForRampUpTest : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

  PeerConnectionWrapperForRampUpTest(
      rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
      rtc::scoped_refptr<PeerConnectionInterface> pc,
      std::unique_ptr<MockPeerConnectionObserver> observer,
      rtc::FakeNetworkManager* fake_network_manager)
      : PeerConnectionWrapper::PeerConnectionWrapper(pc_factory,
                                                     pc,
                                                     std::move(observer)),
        fake_network_manager_(std::move(fake_network_manager)) {}

  bool AddIceCandidates(std::vector<const IceCandidateInterface*> candidates) {
    bool success = true;
    for (const auto candidate : candidates) {
      if (!pc()->AddIceCandidate(candidate)) {
        success = false;
      }
    }
    return success;
  }

  rtc::scoped_refptr<VideoTrackInterface> CreateLocalVideoTrack(
      const FakePeriodicVideoSource::Config& config) {
    video_track_sources_.emplace_back(
        new rtc::RefCountedObject<FakePeriodicVideoTrackSource>(
            config, false /* remote */));
    return rtc::scoped_refptr<VideoTrackInterface>(
        pc_factory()->CreateVideoTrack(rtc::CreateRandomUuid(),
                                       video_track_sources_.back()));
  }

  rtc::scoped_refptr<AudioTrackInterface> CreateLocalAudioTrack(
      const FakeConstraints& constraints) {
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
        pc_factory()->CreateAudioSource(&constraints);
    return pc_factory()->CreateAudioTrack(rtc::CreateRandomUuid(), source);
  }

 private:
  // This is owned by the Test, not the Wrapper. It needs to outlive the
  // Wrapper, because the port allocator expects its lifetime to be longer than
  // the PeerConnection's lifetime.
  rtc::FakeNetworkManager* fake_network_manager_;
  std::vector<std::unique_ptr<webrtc::FakePeriodicVideoSource>>
      fake_video_sources_;
  std::vector<rtc::scoped_refptr<webrtc::VideoTrackSource>>
      video_track_sources_;
};

// TODO(shampson): Paramaterize the test to run for both Plan B & Unified Plan.
class PeerConnectionRampUpTest : public ::testing::Test {
 public:
  PeerConnectionRampUpTest()
      : test_state_(kFirstRampup),
        clock_(Clock::GetRealTimeClock()),
        virtual_socket_server_(new rtc::VirtualSocketServer()),
        firewall_socket_server_(
            new rtc::FirewallSocketServer(virtual_socket_server_.get())),
        network_thread_(new rtc::Thread(firewall_socket_server_.get())),
        worker_thread_(rtc::Thread::Create()) {
    network_thread_->SetName("PCNetworkThread", this);
    worker_thread_->SetName("PCWorkerThread", this);
    RTC_CHECK(network_thread_->Start());
    RTC_CHECK(worker_thread_->Start());

    pc_factory_ = CreatePeerConnectionFactory(
        network_thread_.get(), worker_thread_.get(), rtc::Thread::Current(),
        rtc::scoped_refptr<webrtc::AudioDeviceModule>(
            FakeAudioCaptureModule::Create()),
        CreateBuiltinAudioEncoderFactory(), CreateBuiltinAudioDecoderFactory(),
        CreateBuiltinVideoEncoderFactory(), CreateBuiltinVideoDecoderFactory(),
        nullptr /* audio_mixer */, nullptr /* audio_processing */);
  }

  virtual ~PeerConnectionRampUpTest() {}

  bool CreatePeerConnectionWrappers(const RTCConfiguration& caller_config,
                                    const RTCConfiguration& callee_config) {
    caller_ = CreatePeerConnectionWrapper(caller_config);
    callee_ = CreatePeerConnectionWrapper(callee_config);
    return caller_ && callee_;
  }

  std::unique_ptr<PeerConnectionWrapperForRampUpTest>
  CreatePeerConnectionWrapper(const RTCConfiguration& config) {
    auto* fake_network_manager = new rtc::FakeNetworkManager();
    fake_network_manager->AddInterface(kDefaultLocalAddress);
    fake_network_managers_.emplace_back(fake_network_manager);
    auto port_allocator =
        rtc::MakeUnique<cricket::BasicPortAllocator>(fake_network_manager);

    port_allocator->set_step_delay(cricket::kMinimumStepDelay);
    auto observer = rtc::MakeUnique<MockPeerConnectionObserver>();
    auto pc = pc_factory_->CreatePeerConnection(
        config, std::move(port_allocator), nullptr, observer.get());
    if (!pc) {
      return nullptr;
    }

    return rtc::MakeUnique<PeerConnectionWrapperForRampUpTest>(
        pc_factory_, pc, std::move(observer), fake_network_manager);
  }

  void SetupOneToOneCall() {
    ASSERT_TRUE(caller_);
    ASSERT_TRUE(callee_);

    FakePeriodicVideoSource::Config config;
    // Set max frame rate to 10fps to reduce the risk of test flakiness.
    config.frame_interval_ms = 100;
    caller_->AddTrack(caller_->CreateLocalVideoTrack(config));
    callee_->AddTrack(callee_->CreateLocalVideoTrack(config));
    FakeConstraints constraints;
    // Disable highpass filter so that we can get all the test audio frames.
    constraints.AddMandatory(MediaConstraintsInterface::kHighpassFilter, false);
    caller_->AddTrack(caller_->CreateLocalAudioTrack(constraints));
    callee_->AddTrack(callee_->CreateLocalAudioTrack(constraints));

    // Do the SDP negotiation, and also exchange ice candidates.
    ASSERT_TRUE(caller_->ExchangeOfferAnswerWith(callee_.get()));
    ASSERT_TRUE_WAIT(
        caller_->signaling_state() == webrtc::PeerConnectionInterface::kStable,
        kDefaultTimeout);
    ASSERT_TRUE_WAIT(caller_->IsIceGatheringDone(), kDefaultTimeout);
    ASSERT_TRUE_WAIT(callee_->IsIceGatheringDone(), kDefaultTimeout);

    // Connect an ICE candidate pairs.
    ASSERT_TRUE(
        callee_->AddIceCandidates(caller_->observer()->GetAllCandidates()));
    ASSERT_TRUE(
        caller_->AddIceCandidates(callee_->observer()->GetAllCandidates()));
    // This means that ICE and DTLS are connected.
    ASSERT_TRUE_WAIT(callee_->IsIceConnected(), kDefaultTimeout);
    ASSERT_TRUE_WAIT(caller_->IsIceConnected(), kDefaultTimeout);
  }

  // See how BWE changes over time. This goes through 3 main states:
  // 1) First ramp up. The fake network isn't limited, and we wait for
  // the bandwidth estimation to exceed our expected threshold amount.
  // 2) Ramp down. The fake network becomes limited and we wait for the
  // bandwidth estimation to go below an expected low threshold amount.
  // TODO(bugs.webrtc.org/7668): Add a second rampup when the underlying
  // virtual network used is updated. Currently the second rampup takes
  // about 300 seconds with using Turn over TCP.
  void RunTest(const std::string& test_string) {
    // TODO(shampson): Consider adding a timeout for the test.
    test_string_ = test_string;
    test_start_ms_ = clock_->TimeInMilliseconds();
    do {
      double caller_outgoing_bitrate = GetCallerOutgoingBitrate();
      ASSERT_NE(0, caller_outgoing_bitrate);
      MaybeEvolveTestState(caller_outgoing_bitrate);
      rtc::Thread::Current()->ProcessMessages(kPollIntervalMs);
    } while (test_state_ != kTestDone);
  }

  rtc::Thread* network_thread() { return network_thread_.get(); }

  PeerConnectionWrapperForRampUpTest* caller() { return caller_.get(); }

  PeerConnectionWrapperForRampUpTest* callee() { return callee_.get(); }

 private:
  void MaybeEvolveTestState(double bitrate) {
    int64_t now = clock_->TimeInMilliseconds();
    switch (test_state_) {
      case kFirstRampup:
        if (bitrate >= kExpectedHighBitrateBps) {
          test::PrintResult("pc_ramp_up_down_", test_string_, "first_rampup",
                            now - test_start_ms_, "ms", false);
          virtual_socket_server_->set_bandwidth(kLowBandwidthLimitBps / 8);
          test_state_ = kRampDown;
        }
        break;
      case kRampDown:
        if (bitrate < (kLowBandwidthLimitBps / 2) + kLowBitrateMarginBps) {
          test::PrintResult("pc_ramp_up_down_", test_string_, "rampdown",
                            now - test_start_ms_, "ms", false);
          test_state_ = kTestDone;
          // Setting the bandwidth to 0 means all sends will occur without
          // delay.
          virtual_socket_server_->set_bandwidth(0);
        }
        break;
      case kTestDone:
        break;
    }
  }

  // Gets the caller's outgoing bitrate from the stats. Returns 0 if something
  // went wrong. It takes the outgoing bitrate from the current selected ICE
  // candidate pair's stats.
  double GetCallerOutgoingBitrate() {
    auto transport_stats =
        caller_->GetStats()->GetStatsOfType<RTCTransportStats>();
    if (transport_stats.size() == 0u ||
        !transport_stats[0]->selected_candidate_pair_id.is_defined()) {
      return 0;
    }
    std::string selected_ice_candidate_pair_id =
        transport_stats[0]->selected_candidate_pair_id.ValueToString();
    for (const auto& ice_candidate_pair_stats :
         caller_->GetStats()->GetStatsOfType<RTCIceCandidatePairStats>()) {
      if (ice_candidate_pair_stats->id() == selected_ice_candidate_pair_id &&
          ice_candidate_pair_stats->available_outgoing_bitrate.is_defined()) {
        // We've found the selected candidate pair to grab the outgoing
        // bandwidth estimate stat from.
        return *ice_candidate_pair_stats->available_outgoing_bitrate;
      }
    }
    // We couldn't get the |available_outgoing_bitrate| for the active candidate
    // pair.
    return 0;
  }

  enum TestStates {
    kFirstRampup,
    kRampDown,
    kTestDone,
  };

  TestStates test_state_;
  int64_t test_start_ms_;
  std::string test_string_;
  Clock* const clock_;

  // |virtual_socket_server_| is used by |network_thread_| so it must be
  // destroyed later.
  // TODO(bugs.webrtc.org/7668): We would like to update the virtual network we
  // use for this test. VirtualSocketServer isn't ideal because:
  // 1) It uses the same queue & network capacity for both directions.
  // 2) VirtualSocketServer implements how the network bandwidth affects the
  //    send delay differently than the SimulatedNetwork, used by the
  //    FakeNetworkPipe. It would be ideal if all of levels of virtual
  //    networks used in testing were consistent.
  std::unique_ptr<rtc::VirtualSocketServer> virtual_socket_server_;
  std::unique_ptr<rtc::FirewallSocketServer> firewall_socket_server_;
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  // The |pc_factory| uses |network_thread_| & |worker_thread_|, so it must be
  // destroyed first.
  std::vector<std::unique_ptr<rtc::FakeNetworkManager>> fake_network_managers_;
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
  std::unique_ptr<PeerConnectionWrapperForRampUpTest> caller_;
  std::unique_ptr<PeerConnectionWrapperForRampUpTest> callee_;
};

TEST_F(PeerConnectionRampUpTest, TurnOverTCP) {
  static const rtc::SocketAddress turn_server_internal_address{"88.88.88.0",
                                                               3478};
  static const rtc::SocketAddress turn_server_external_address{"88.88.88.1", 0};
  // Enable TCP for the fake turn server.
  cricket::TestTurnServer turn_server(
      network_thread(), turn_server_internal_address,
      turn_server_external_address, cricket::PROTO_TCP);
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.push_back("turn:88.88.88.0:3478?transport=tcp");
  ice_server.username = "test";
  ice_server.password = "test";
  PeerConnectionInterface::RTCConfiguration client_1_config;
  client_1_config.servers.push_back(ice_server);
  client_1_config.type = webrtc::PeerConnectionInterface::kRelay;
  PeerConnectionInterface::RTCConfiguration client_2_config;
  client_2_config.servers.push_back(ice_server);
  client_2_config.type = webrtc::PeerConnectionInterface::kRelay;
  ASSERT_TRUE(CreatePeerConnectionWrappers(client_1_config, client_2_config));

  SetupOneToOneCall();
  RunTest("turn_over_tcp");
}

// TODO(bugs.webrtc.org/7668): Test other ICE configurations.

}  // namespace webrtc
