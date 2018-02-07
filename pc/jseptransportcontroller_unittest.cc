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
using webrtc::SdpType;

static const int kTimeout = 100;
static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";
// static const char kIceUfrag2[] = "TESTICEUFRAG0002";
// static const char kIcePwd2[] = "TESTICEPWD00000000000002";
// static const char kIceUfrag3[] = "TESTICEUFRAG0003";
// static const char kIcePwd3[] = "TESTICEPWD00000000000003";
static const char kAudioMid1[] = "audio1";
static const char kAudioMid2[] = "audio2";
static const char kVideoMid1[] = "video1";

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
    return rtc::MakeUnique<FakeDtlsTransport>(std::move(ice));
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

 protected:
  std::unique_ptr<JsepTransportController> transport_controller_;

 private:
  std::unique_ptr<FakeTransportFactory> fake_transport_factory_;
  rtc::Thread* const signaling_thread_ = nullptr;
};

TEST_F(JsepTransportControllerTest, TestSetIceConfig) {
  JsepTransportController::Config config;
  CreateJsepTransport(config);
  auto description = CreateSessionDescriptionWithDefaultAudioVideo();
  auto error = transport_controller_->SetLocalDescription(SdpType::kOffer,
                                                          description.get());
  EXPECT_TRUE(error.ok());

  transport_controller_->SetIceConfig(
      CreateIceConfig(kTimeout, cricket::GATHER_CONTINUALLY));
  FakeDtlsTransport* fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  ASSERT_NE(nullptr, fake_audio_dtls);
  EXPECT_EQ(kTimeout,
            fake_audio_dtls->fake_ice_transport()->receiving_timeout());
  EXPECT_TRUE(fake_audio_dtls->fake_ice_transport()->gather_continually());

  TransportDescription transport_desc(std::vector<std::string>(), kIceUfrag1,
                                      kIcePwd1, cricket::ICEMODE_FULL,
                                      cricket::CONNECTIONROLE_ACTPASS, nullptr);
  AddAudioSection(description.get(), kAudioMid2, transport_desc);

  error = transport_controller_->SetLocalDescription(SdpType::kOffer,
                                                     description.get());
  EXPECT_TRUE(error.ok());
  fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid2));
  ASSERT_NE(nullptr, fake_audio_dtls);
  EXPECT_EQ(kTimeout,
            fake_audio_dtls->fake_ice_transport()->receiving_timeout());
  EXPECT_TRUE(fake_audio_dtls->fake_ice_transport()->gather_continually());
}

}  // namespace webrtc
