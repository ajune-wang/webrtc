/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/peerconnectionproxy.h"
#include "p2p/base/fakeportallocator.h"
#include "p2p/base/teststunserver.h"
#include "p2p/client/basicportallocator.h"
#include "pc/mediasession.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/sdputils.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "pc/test/fakeaudiocapturemodule.h"
#include "rtc_base/fakenetwork.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/virtualsocketserver.h"

namespace webrtc {

using BundlePolicy = PeerConnectionInterface::BundlePolicy;
using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;
using RtcpMuxPolicy = PeerConnectionInterface::RtcpMuxPolicy;
using ::testing::Values;
using rtc::SocketAddress;

constexpr int kDefaultTimeout = 10000;

class PeerConnectionWrapperForBundleUnitTest : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

  bool AddIceCandidateToMedia(cricket::Candidate* candidate,
                              cricket::MediaType media_type) {
    auto* desc = pc()->remote_description()->description();
    for (size_t i = 0; i < desc->contents().size(); i++) {
      const auto& content = desc->contents()[i];
      auto* media_desc =
          static_cast<cricket::MediaContentDescription*>(content.description);
      if (media_desc->type() == media_type) {
        candidate->set_transport_name(content.name);
        JsepIceCandidate jsep_candidate(content.name, i, *candidate);
        return pc()->AddIceCandidate(&jsep_candidate);
      }
    }
    RTC_NOTREACHED();
    return false;
  }

  rtc::PacketTransportInternal* voice_rtp_transport_channel() {
    return (voice_channel() ? voice_channel()->rtp_dtls_transport() : nullptr);
  }

  rtc::PacketTransportInternal* voice_rtcp_transport_channel() {
    return (voice_channel() ? voice_channel()->rtcp_dtls_transport() : nullptr);
  }

  cricket::VoiceChannel* voice_channel() {
    return GetInternalPeerConnection()->voice_channel();
  }

  rtc::PacketTransportInternal* video_rtp_transport_channel() {
    return (video_channel() ? video_channel()->rtp_dtls_transport() : nullptr);
  }

  rtc::PacketTransportInternal* video_rtcp_transport_channel() {
    return (video_channel() ? video_channel()->rtcp_dtls_transport() : nullptr);
  }

  cricket::VideoChannel* video_channel() {
    return GetInternalPeerConnection()->video_channel();
  }

  PeerConnection* GetInternalPeerConnection() {
    auto* pci = reinterpret_cast<
        PeerConnectionProxyWithInternal<PeerConnectionInterface>*>(pc());
    return reinterpret_cast<PeerConnection*>(pci->internal());
  }

  // Returns true if the stats indicate that an ICE connection is either in
  // progress or established with the given remote address.
  bool HasConnectionWithRemoteAddress(const SocketAddress& address) {
    auto report = GetStats();
    if (!report) {
      return false;
    }
    std::string matching_candidate_id;
    for (auto* ice_candidate_stats :
         report->GetStatsOfType<RTCRemoteIceCandidateStats>()) {
      if (*ice_candidate_stats->ip == address.HostAsURIString() &&
          *ice_candidate_stats->port == address.port()) {
        matching_candidate_id = ice_candidate_stats->id();
        break;
      }
    }
    if (matching_candidate_id.empty()) {
      return false;
    }
    for (auto* pair_stats :
         report->GetStatsOfType<RTCIceCandidatePairStats>()) {
      if (*pair_stats->remote_candidate_id == matching_candidate_id) {
        if (*pair_stats->state == RTCStatsIceCandidatePairState::kInProgress ||
            *pair_stats->state == RTCStatsIceCandidatePairState::kSucceeded) {
          return true;
        }
      }
    }
    return false;
  }

  rtc::FakeNetworkManager* network() { return network_; }

  void set_network(rtc::FakeNetworkManager* network) { network_ = network; }

 private:
  rtc::FakeNetworkManager* network_;
};

class PeerConnectionBundleUnitTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForBundleUnitTest> WrapperPtr;

  PeerConnectionBundleUnitTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
#ifdef WEBRTC_ANDROID
    InitializeAndroidObjects();
#endif
    pc_factory_ = CreatePeerConnectionFactory(
        rtc::Thread::Current(), rtc::Thread::Current(), rtc::Thread::Current(),
        FakeAudioCaptureModule::Create(), nullptr, nullptr);
  }

  WrapperPtr CreatePeerConnection() {
    return CreatePeerConnection(RTCConfiguration());
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    auto* fake_network = NewFakeNetwork();
    auto port_allocator =
        rtc::MakeUnique<cricket::BasicPortAllocator>(fake_network);
    port_allocator->set_flags(cricket::PORTALLOCATOR_DISABLE_TCP |
                              cricket::PORTALLOCATOR_DISABLE_RELAY);
    port_allocator->set_step_delay(cricket::kMinimumStepDelay);
    auto observer = rtc::MakeUnique<MockPeerConnectionObserver>();
    auto pc = pc_factory_->CreatePeerConnection(
        config, std::move(port_allocator), nullptr, observer.get());
    if (!pc) {
      return nullptr;
    }

    auto wrapper = rtc::MakeUnique<PeerConnectionWrapperForBundleUnitTest>(
        pc_factory_, pc, std::move(observer));
    wrapper->set_network(fake_network);
    return wrapper;
  }

  // Accepts the same arguments as CreatePeerConnection and adds default audio
  // and video tracks.
  template <typename... Args>
  WrapperPtr CreatePeerConnectionWithAudioVideo(Args&&... args) {
    auto wrapper = CreatePeerConnection(std::forward<Args>(args)...);
    if (!wrapper) {
      return nullptr;
    }
    wrapper->AddAudioVideoStream("s", "a", "v");
    return wrapper;
  }

  // Returns a SocketAddress with a consistently generated and unique host and
  // port.
  SocketAddress NewClientAddress() {
    // Generate a highly visible host.
    int n = address_counter_++;
    std::stringstream ss;
    ss << n << "." << n << "." << n << "." << n;
    // Note that the port must be >= 1024 or else it will be rejected. See
    // cricket::VerifyCandidate.
    int port = port_counter_;
    port_counter_ += 1111;
    return SocketAddress(ss.str(), port);
  }

  cricket::Candidate CreateLocalUdpCandidate(
      const rtc::SocketAddress& address) {
    cricket::Candidate candidate;
    candidate.set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);
    candidate.set_protocol(cricket::UDP_PROTOCOL_NAME);
    candidate.set_address(address);
    candidate.set_type(cricket::LOCAL_PORT_TYPE);
    return candidate;
  }

  rtc::FakeNetworkManager* NewFakeNetwork() {
    // The PeerConnection's port allocator is tied to the PeerConnection's
    // lifetime and expects the underlying NetworkManager to outlive it. That
    // prevents us from having the PeerConnectionWrapper own the fake network.
    // Therefore, the test fixture will own all the fake networks even though
    // tests should access the fake network through the PeerConnectionWrapper.
    auto* fake_network = new rtc::FakeNetworkManager();
    fake_networks_.emplace_back(fake_network);
    return fake_network;
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
  std::vector<std::unique_ptr<rtc::FakeNetworkManager>> fake_networks_;

  int address_counter_ = 1;
  int port_counter_ = 4321;
};

SdpContentMutator RemoveRtcpMux() {
  return [](cricket::ContentInfo* content, cricket::TransportInfo* transport) {
    auto* media_desc =
        static_cast<cricket::MediaContentDescription*>(content->description);
    media_desc->set_rtcp_mux(false);
  };
}

// Test that there are 2 local UDP candidates (1 RTP and 1 RTCP candidate) for
// each media section when disabling bundle and disabling RTCP multiplexing.
TEST_F(PeerConnectionBundleUnitTest,
       TwoCandidatesForEachTransportWhenNoBundleNoRtcpMux) {
  RTCConfiguration config;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  caller->network()->AddInterface(NewClientAddress());
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  RTCOfferAnswerOptions options_no_bundle;
  options_no_bundle.use_rtp_mux = false;
  auto answer = callee->CreateAnswer(options_no_bundle);
  SdpContentsForEach(RemoveRtcpMux(), answer->description());
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));

  EXPECT_TRUE_WAIT(caller->IsIceGatheringDone(), kDefaultTimeout);

  EXPECT_EQ(2u, caller->observer()->GetCandidatesByMline(0).size());
  EXPECT_EQ(2u, caller->observer()->GetCandidatesByMline(1).size());
}

// Test that there is 1 local UDP candidate for both RTP and RTCP for each media
// section when disabling bundle but enabling RTCP multiplexing.
TEST_F(PeerConnectionBundleUnitTest,
       OneCandidateForEachTransportWhenNoBundleButRtcpMux) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  caller->network()->AddInterface(NewClientAddress());
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  RTCOfferAnswerOptions options_no_bundle;
  options_no_bundle.use_rtp_mux = false;
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswer(options_no_bundle)));

  EXPECT_TRUE_WAIT(caller->IsIceGatheringDone(), kDefaultTimeout);

  EXPECT_EQ(1u, caller->observer()->GetCandidatesByMline(0).size());
  EXPECT_EQ(1u, caller->observer()->GetCandidatesByMline(1).size());
}

// Test that there is 1 local UDP candidate in only the first media section when
// bundling and enabling RTCP multiplexing.
TEST_F(PeerConnectionBundleUnitTest,
       OneCandidateOnlyOnFirstTransportWhenBundleAndRtcpMux) {
  RTCConfiguration config;
  config.bundle_policy = BundlePolicy::kBundlePolicyMaxBundle;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  caller->network()->AddInterface(NewClientAddress());
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(caller->SetRemoteDescription(callee->CreateAnswer()));

  EXPECT_TRUE_WAIT(caller->IsIceGatheringDone(), kDefaultTimeout);

  EXPECT_EQ(1u, caller->observer()->GetCandidatesByMline(0).size());
  EXPECT_EQ(0u, caller->observer()->GetCandidatesByMline(1).size());
}

// The following parameterized test verifies that an offer/answer with varying
// bundle policies and either bundle in the answer or not will produce the
// expected RTP transports for audio and video. In particular, for bundling we
// care about whether they are separate transports or the same.

enum class BundleIncluded { kBundleInAnswer, kBundleNotInAnswer };
std::ostream& operator<<(std::ostream& out, BundleIncluded value) {
  switch (value) {
    case BundleIncluded::kBundleInAnswer:
      return out << "bundle in answer";
    case BundleIncluded::kBundleNotInAnswer:
      return out << "bundle not in answer";
  }
  return out << "unknown";
}

class PeerConnectionBundleMatrixUnitTest
    : public PeerConnectionBundleUnitTest,
      public ::testing::WithParamInterface<
          std::tuple<BundlePolicy, BundleIncluded, bool, bool>> {
 protected:
  PeerConnectionBundleMatrixUnitTest() {
    bundle_policy_ = std::get<0>(GetParam());
    bundle_included_ = std::get<1>(GetParam());
    expected_same_before_ = std::get<2>(GetParam());
    expected_same_after_ = std::get<3>(GetParam());
  }

  PeerConnectionInterface::BundlePolicy bundle_policy_;
  BundleIncluded bundle_included_;
  bool expected_same_before_;
  bool expected_same_after_;
};

TEST_P(PeerConnectionBundleMatrixUnitTest,
       VerifyTransportsBeforeAndAfterSettingRemoteAnswer) {
  RTCConfiguration config;
  config.bundle_policy = bundle_policy_;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  bool equal_before = (caller->voice_rtp_transport_channel() ==
                       caller->video_rtp_transport_channel());
  EXPECT_EQ(expected_same_before_, equal_before);

  RTCOfferAnswerOptions options;
  options.use_rtp_mux = (bundle_included_ == BundleIncluded::kBundleInAnswer);
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal(options)));
  bool equal_after = (caller->voice_rtp_transport_channel() ==
                      caller->video_rtp_transport_channel());
  EXPECT_EQ(expected_same_after_, equal_after);
}

INSTANTIATE_TEST_CASE_P(
    PeerConnectionBundleUnitTest,
    PeerConnectionBundleMatrixUnitTest,
    Values(std::make_tuple(BundlePolicy::kBundlePolicyBalanced,
                           BundleIncluded::kBundleInAnswer,
                           false,
                           true),
           std::make_tuple(BundlePolicy::kBundlePolicyBalanced,
                           BundleIncluded::kBundleNotInAnswer,
                           false,
                           false),
           std::make_tuple(BundlePolicy::kBundlePolicyMaxBundle,
                           BundleIncluded::kBundleInAnswer,
                           true,
                           true),
           std::make_tuple(BundlePolicy::kBundlePolicyMaxBundle,
                           BundleIncluded::kBundleNotInAnswer,
                           true,
                           true),
           std::make_tuple(BundlePolicy::kBundlePolicyMaxCompat,
                           BundleIncluded::kBundleInAnswer,
                           false,
                           true),
           std::make_tuple(BundlePolicy::kBundlePolicyMaxCompat,
                           BundleIncluded::kBundleNotInAnswer,
                           false,
                           false)));

TEST_F(PeerConnectionBundleUnitTest,
       TransportsSameForMaxBundleWithBundleInRemoteOffer) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  RTCConfiguration config;
  config.bundle_policy = BundlePolicy::kBundlePolicyMaxBundle;
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  RTCOfferAnswerOptions options_with_bundle;
  options_with_bundle.use_rtp_mux = true;
  ASSERT_TRUE(callee->SetRemoteDescription(
      caller->CreateOfferAndSetAsLocal(options_with_bundle)));

  EXPECT_EQ(callee->voice_rtp_transport_channel(),
            callee->video_rtp_transport_channel());

  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));

  EXPECT_EQ(callee->voice_rtp_transport_channel(),
            callee->video_rtp_transport_channel());
}

TEST_F(PeerConnectionBundleUnitTest,
       FailToSetRemoteOfferWithNoBundleWhenBundlePolicyMaxBundle) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  RTCConfiguration config;
  config.bundle_policy = BundlePolicy::kBundlePolicyMaxBundle;
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  RTCOfferAnswerOptions options_no_bundle;
  options_no_bundle.use_rtp_mux = false;
  EXPECT_FALSE(callee->SetRemoteDescription(
      caller->CreateOfferAndSetAsLocal(options_no_bundle)));
}

// Test that if the media section which has the bundled transport is rejected,
// then the peers still connect and the bundled transport switches to the other
// media section.
TEST_F(PeerConnectionBundleUnitTest,
       SuccessfullyNegotiateMaxBundleIfBundleTransportMediaRejected) {
  RTCConfiguration config;
  config.bundle_policy = BundlePolicy::kBundlePolicyMaxBundle;
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();
  callee->AddVideoStream("vs", "v");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 0;
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal(options)));

  EXPECT_FALSE(caller->voice_rtp_transport_channel());
  EXPECT_TRUE(caller->video_rtp_transport_channel());
}

// When requiring RTCP multiplexing, the PeerConnection never makes RTCP
// transport channels.
TEST_F(PeerConnectionBundleUnitTest,
       NeverCreateRtcpTransportWithRtcpMuxRequired) {
  RTCConfiguration config;
  config.rtcp_mux_policy = RtcpMuxPolicy::kRtcpMuxPolicyRequire;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_FALSE(caller->voice_rtcp_transport_channel());
  EXPECT_FALSE(caller->video_rtcp_transport_channel());

  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  EXPECT_FALSE(caller->voice_rtcp_transport_channel());
  EXPECT_FALSE(caller->video_rtcp_transport_channel());
}

// When negotiating RTCP multiplexing, the PeerConnection makes RTCP transport
// channels when the offer is sent, but will destroy them once the remote answer
// is set.
TEST_F(PeerConnectionBundleUnitTest,
       CreateRtcpTransportOnlyBeforeAnswerWithRtcpMuxNegotiate) {
  RTCConfiguration config;
  config.rtcp_mux_policy = RtcpMuxPolicy::kRtcpMuxPolicyNegotiate;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_TRUE(caller->voice_rtcp_transport_channel());
  EXPECT_TRUE(caller->video_rtcp_transport_channel());

  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  EXPECT_FALSE(caller->voice_rtcp_transport_channel());
  EXPECT_FALSE(caller->video_rtcp_transport_channel());
}

TEST_F(PeerConnectionBundleUnitTest,
       FailToSetDescriptionWithBundleAndNoRtcpMux) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  auto offer = caller->CreateOffer(options);
  SdpContentsForEach(RemoveRtcpMux(), offer->description());

  std::string error;
  EXPECT_FALSE(caller->SetLocalDescription(CloneSessionDescription(offer.get()),
                                           &error));
  EXPECT_EQ(
      "Failed to set local offer sdp: RTCP-MUX must be enabled when BUNDLE is "
      "enabled.",
      error);

  EXPECT_FALSE(callee->SetRemoteDescription(std::move(offer), &error));
  EXPECT_EQ(
      "Failed to set remote offer sdp: RTCP-MUX must be enabled when BUNDLE is "
      "enabled.",
      error);
}

// Test that candidates sent to the "video" transport do not get pushed down to
// the "audio" transport channel when bundling.
TEST_F(PeerConnectionBundleUnitTest,
       IgnoreCandidatesForUnusedTransportWhenBundling) {
  const SocketAddress kAudioAddress1 = NewClientAddress();
  const SocketAddress kAudioAddress2 = NewClientAddress();
  const SocketAddress kVideoAddress = NewClientAddress();

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  caller->network()->AddInterface(NewClientAddress());
  callee->network()->AddInterface(NewClientAddress());

  RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  // The way the *_WAIT checks work is they only wait if the condition fails,
  // which does not help in the case where state is not changing. This is
  // problematic in this test since we want to verify that adding a video
  // candidate does _not_ change state. So we interleave candidates and assume
  // that messages are executed in the order they were posted.

  cricket::Candidate audio_candidate1 = CreateLocalUdpCandidate(kAudioAddress1);
  ASSERT_TRUE(caller->AddIceCandidateToMedia(&audio_candidate1,
                                             cricket::MEDIA_TYPE_AUDIO));

  cricket::Candidate video_candidate = CreateLocalUdpCandidate(kVideoAddress);
  ASSERT_TRUE(caller->AddIceCandidateToMedia(&video_candidate,
                                             cricket::MEDIA_TYPE_VIDEO));

  cricket::Candidate audio_candidate2 = CreateLocalUdpCandidate(kAudioAddress2);
  ASSERT_TRUE(caller->AddIceCandidateToMedia(&audio_candidate2,
                                             cricket::MEDIA_TYPE_AUDIO));

  EXPECT_TRUE_WAIT(caller->HasConnectionWithRemoteAddress(kAudioAddress1),
                   kDefaultTimeout);
  EXPECT_TRUE_WAIT(caller->HasConnectionWithRemoteAddress(kAudioAddress2),
                   kDefaultTimeout);
  EXPECT_FALSE(caller->HasConnectionWithRemoteAddress(kVideoAddress));
}

}  // namespace webrtc
