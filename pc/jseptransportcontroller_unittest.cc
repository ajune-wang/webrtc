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

#include "p2p/base/fakedtlstransport.h"
#include "p2p/base/fakeicetransport.h"
#include "p2p/base/transportfactoryinterface.h"
#include "p2p/base/transportinfo.h"
#include "pc/jseptransportcontroller.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

using cricket::AudioContentDescription;
using cricket::FakeIceTransport;
using cricket::FakeDtlsTransport;
using cricket::TransportDescription;
using cricket::TransportInfo;
using cricket::VideoContentDescription;
using cricket::DtlsTransportInternal;
using cricket::IceTransportInternal;
using cricket::IceConnectionState;
using cricket::IceGatheringState;
using cricket::Candidate;
using cricket::Candidates;
using webrtc::SdpType;
using cricket::kIceConnectionConnecting;
using cricket::kIceGatheringNew;
using cricket::kIceGatheringGathering;

// static const int kTimeout = 100;
static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";
// static const char kIceUfrag2[] = "TESTICEUFRAG0002";
// static const char kIcePwd2[] = "TESTICEPWD00000000000002";
// static const char kIceUfrag3[] = "TESTICEUFRAG0003";
// static const char kIcePwd3[] = "TESTICEPWD00000000000003";
static const char kAudioMid1[] = "audio1";
// static const char kAudioMid2[] = "audio2";
static const char kVideoMid1[] = "video1";
// static const char kVideoMid2[] = "video2";

namespace webrtc {

class FakeTransportFactory : public cricket::TransportFactoryInterface {
 public:
  std::unique_ptr<IceTransportInternal> CreateIceTransport(
      const std::string& transport_name,
      int component) override {
    return rtc::MakeUnique<FakeIceTransport>(transport_name, component);
  }

  std::unique_ptr<DtlsTransportInternal> CreateDtlsTransport(
      std::unique_ptr<IceTransportInternal> ice,
      const rtc::CryptoOptions& crypto_options) override {
    std::unique_ptr<FakeIceTransport> fake_ice(
        static_cast<FakeIceTransport*>(ice.release()));
    return rtc::MakeUnique<FakeDtlsTransport>(std::move(fake_ice));
  }
};

class JsepTransportControllerTest : public testing::Test,
                                    public sigslot::has_slots<> {
 public:
  JsepTransportControllerTest() : signaling_thread_(rtc::Thread::Current()) {
    fake_transport_factory_ = rtc::MakeUnique<FakeTransportFactory>();
  }

  void CreateJsepTransport(
      JsepTransportController::Config config,
      rtc::Thread* signaling_thread = rtc::Thread::Current(),
      rtc::Thread* network_thread = rtc::Thread::Current(),
      cricket::PortAllocator* port_allocator = nullptr) {
    // The tests only works with |fake_transport_factory|;
    config.external_transport_factory = fake_transport_factory_.get();
    transport_controller_ = rtc::MakeUnique<JsepTransportController>(
        signaling_thread, network_thread, port_allocator, config);
    ConnectTransportControllerSignals();
  }

  void ConnectTransportControllerSignals() {
    transport_controller_->SignalIceConnectionState.connect(
        this, &JsepTransportControllerTest::OnConnectionState);
    transport_controller_->SignalIceGatheringState.connect(
        this, &JsepTransportControllerTest::OnGatheringState);
    transport_controller_->SignalIceCandidatesGathered.connect(
        this, &JsepTransportControllerTest::OnCandidatesGathered);
  }

  std::unique_ptr<cricket::SessionDescription>
  CreateSessionDescriptionWithDefaultAudioVideo() {
    auto description = rtc::MakeUnique<cricket::SessionDescription>();

    TransportDescription transport_desc(
        std::vector<std::string>(), kIceUfrag1, kIcePwd1, cricket::ICEMODE_FULL,
        cricket::CONNECTIONROLE_ACTPASS, nullptr);
    std::unique_ptr<AudioContentDescription> audio(
        new AudioContentDescription());
    description->AddContent(kAudioMid1, cricket::MediaProtocolType::kRtp,
                            /*rejected=*/false, audio.release());
    description->AddTransportInfo(TransportInfo(kAudioMid1, transport_desc));
    std::unique_ptr<VideoContentDescription> video(
        new VideoContentDescription());
    description->AddContent(kVideoMid1, cricket::MediaProtocolType::kRtp,
                            /*rejected=*/false, video.release());
    description->AddTransportInfo(TransportInfo(kVideoMid1, transport_desc));

    return description;
  }

  void AddAudioSection(cricket::SessionDescription* description,
                       const std::string& mid,
                       const TransportDescription& transport_desc) {
    std::unique_ptr<AudioContentDescription> audio(
        new AudioContentDescription());
    description->AddContent(mid, cricket::MediaProtocolType::kRtp,
                            /*rejected=*/false, audio.release());
    description->AddTransportInfo(TransportInfo(mid, transport_desc));
  }

  void AddVideoSection(cricket::SessionDescription* description,
                       const std::string& mid,
                       const TransportDescription& transport_desc) {
    std::unique_ptr<AudioContentDescription> video(
        new AudioContentDescription());
    description->AddContent(mid, cricket::MediaProtocolType::kRtp,
                            /*rejected=*/false, video.release());
    description->AddTransportInfo(TransportInfo(mid, transport_desc));
  }

  cricket::IceConfig CreateIceConfig(
      int receiving_timeout,
      cricket::ContinualGatheringPolicy continual_gathering_policy) {
    cricket::IceConfig config;
    config.receiving_timeout = receiving_timeout;
    config.continual_gathering_policy = continual_gathering_policy;
    return config;
  }

  Candidate CreateCandidate(const std::string& transport_name, int component) {
    Candidate c;
    c.set_transport_name(transport_name);
    c.set_address(rtc::SocketAddress("192.168.1.1", 8000));
    c.set_component(1);
    c.set_protocol(cricket::UDP_PROTOCOL_NAME);
    c.set_priority(1);
    return c;
  }

 protected:
  void OnConnectionState(IceConnectionState state) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    connection_state_ = state;
    ++connection_state_signal_count_;
  }

  void OnGatheringState(IceGatheringState state) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    gathering_state_ = state;
    ++gathering_state_signal_count_;
  }

  void OnCandidatesGathered(const std::string& transport_name,
                            const Candidates& candidates) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    candidates_[transport_name].insert(candidates_[transport_name].end(),
                                       candidates.begin(), candidates.end());
    ++candidates_signal_count_;
  }

  // Information received from signals from transport controller.
  IceConnectionState connection_state_ = kIceConnectionConnecting;
  bool receiving_ = false;
  IceGatheringState gathering_state_ = kIceGatheringNew;
  // transport_name => candidates
  std::map<std::string, Candidates> candidates_;
  // Counts of each signal emitted.
  int connection_state_signal_count_ = 0;
  int receiving_signal_count_ = 0;
  int gathering_state_signal_count_ = 0;
  int candidates_signal_count_ = 0;

  std::unique_ptr<JsepTransportController> transport_controller_;
  std::unique_ptr<FakeTransportFactory> fake_transport_factory_;
  rtc::Thread* const signaling_thread_ = nullptr;
  bool signaled_on_non_signaling_thread_ = false;
};

}  // namespace webrtc
