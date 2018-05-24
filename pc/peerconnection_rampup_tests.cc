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
#include "pc/test/fakeperiodicvideosource.h"
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
// This constant is taken from rampup_tests.cc: kExpectedHighVideoBitrateBps +
// kExpectedHighAudioBitrateBps.
static const int kExpectedHighBitrateBps = 1100000;
static const int kLowBandwidthLimitBps = 20000;
static const int kLowBitrateMarginBps = 20000;
}  // namespace

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;

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

  ~PeerConnectionWrapperForRampUpTest() {
    // Tear down video sources in the proper order.
    for (const auto& video_source : fake_video_sources_) {
      // No more calls to downstream OnFrame
      video_source->Stop();
    }
    for (const auto& track_source : video_track_sources_) {
      // No more calls to upstream AddOrUpdateSink
      track_source->OnSourceDestroyed();
    }
    fake_video_sources_.clear();
    video_track_sources_.clear();
  }

  void AddNetworkInterface(const rtc::SocketAddress address) {
    fake_network_manager_->AddInterface(address);
  }

  // Creates a local video track. A local reference of it and the fake video
  // source are kept in PeerConnectionWrapperForRampUpTest so that we can
  // properly stop the source and call OnSourceDestroyed when the test is torn
  // down.
  rtc::scoped_refptr<VideoTrackInterface> CreateLocalVideoTrack(
      const FakePeriodicVideoSource::Config& config) {
    fake_video_sources_.emplace_back(
        rtc::MakeUnique<FakePeriodicVideoSource>(config));
    video_track_sources_.emplace_back(
        new rtc::RefCountedObject<VideoTrackSource>(
            fake_video_sources_.back().get(), false /* remote */));
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
      : debug_(false),
        test_state_(kFirstRampup),
        stop_polling_event_(false, false),
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
    // Connect an ICE candidate pair.
    auto caller_candidate = caller_->observer()->last_candidate();
    auto callee_candidate = callee_->observer()->last_candidate();
    ASSERT_TRUE(caller_candidate);
    ASSERT_TRUE(callee_candidate);
    callee_->pc()->AddIceCandidate(caller_candidate);
    caller_->pc()->AddIceCandidate(callee_candidate);
    // This means that ICE and DTLS are connected.
    ASSERT_TRUE_WAIT(callee_->IsIceConnected(), 3000);
    ASSERT_TRUE_WAIT(caller_->IsIceConnected(), 3000);

    auto caller_receivers = caller_->observer()->GetAddTrackReceivers();
    ASSERT_EQ(2u, caller_receivers.size());
    for (const auto& receiver : caller_receivers) {
      if (receiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
        rtc::scoped_refptr<VideoTrackInterface> video_track(
            static_cast<VideoTrackInterface*>(receiver->track().get()));
        caller_fake_video_renderer_ =
            rtc::MakeUnique<FakeVideoTrackRenderer>(video_track);
      }
    }
  }

  // See how BWE changes over time. This kicks off the polling thread, that
  // polls the caller's send BWE stat every 50 ms. This goes through 3 main
  // states:
  // 1) First ramp up. The fake network isn't limited, and we wait for
  // the bandwidth estimation to exceed our expected threshold amount.
  // 2) Ramp down. The fake network becomes limited and we wait for the
  // bandwidth estimation to go below an expected low threshold amount.
  // 3) Second ramp up. The fake network is unlimited again and we wait for the
  // bandwidth to again go above the expected threshold.
  void RunTest(const std::string& test_string) {
    // TODO(shampson): Consider adding a timeout for the test.
    test_string_ = test_string;
    test_start_ms_ = clock_->TimeInMilliseconds();
    do {
      double caller_outgoing_bitrate = GetCallerOutgoingBitrate();
      MaybeEvolveTestState(caller_outgoing_bitrate);
      if (debug_) {
        RTC_DCHECK(caller_fake_video_renderer_);
        printf("rendered frames: %d\n",
               caller_fake_video_renderer_->num_rendered_frames());
      }
    } while (!stop_polling_event_.Wait(kPollIntervalMs));
  }

  rtc::Thread* network_thread() { return network_thread_.get(); }

  PeerConnectionWrapperForRampUpTest* caller() { return caller_.get(); }

  PeerConnectionWrapperForRampUpTest* callee() { return callee_.get(); }

  void set_debug(bool debug) { debug_ = debug; }

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
        if (bitrate < kLowBandwidthLimitBps + kLowBitrateMarginBps) {
          test::PrintResult("pc_ramp_up_down_", test_string_, "rampdown",
                            now - test_start_ms_, "ms", false);
          test_state_ = kSecondRampUp;
          // Setting the bandwidth to 0 means all sends will occur without
          // delay.
          virtual_socket_server_->set_bandwidth(0);
        }
        break;
      case kSecondRampUp:
        if (bitrate >= kExpectedHighBitrateBps) {
          test::PrintResult("pc_ramp_up_down_", test_string_, "second_rampup",
                            now - test_start_ms_, "ms", false);
          test_state_ = kTestDone;
        }
        break;
      case kTestDone:
        stop_polling_event_.Set();
        break;
    }
  }

  // Gets the caller's outgoing bitrate from the stats. Returns 0 if something
  // went wrong.
  double GetCallerOutgoingBitrate() {
    auto ice_candidate_pair_stats =
        caller_->GetStats()->GetStatsOfType<RTCIceCandidatePairStats>();
    if (ice_candidate_pair_stats.size() == 0u ||
        !ice_candidate_pair_stats[0]->available_outgoing_bitrate.is_defined()) {
      return 0;
    }
    return *ice_candidate_pair_stats[0]->available_outgoing_bitrate;
  }

  enum TestStates {
    kFirstRampup,
    kRampDown,
    kSecondRampUp,
    kTestDone,
  };

  bool debug_;
  TestStates test_state_;
  int64_t test_start_ms_;
  std::string test_string_;
  rtc::Event stop_polling_event_;
  Clock* const clock_;

  // |virtual_socket_server_| is used by |network_thread_| so it must be
  // destroyed later.
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
  std::unique_ptr<FakeVideoTrackRenderer> caller_fake_video_renderer_;
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
  caller()->AddNetworkInterface(kDefaultLocalAddress);
  callee()->AddNetworkInterface(kDefaultLocalAddress);

  SetupOneToOneCall();
  RunTest("turn_over_tcp");
}

// TODO(bugs.webrtc.org/7668): Test other ICE configurations.

}  // namespace webrtc
