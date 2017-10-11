/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <tuple>

#include "api/peerconnectionproxy.h"
#include "media/base/fakemediaengine.h"
#include "pc/mediasession.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionfactory.h"
#include "pc/peerconnectionwrapper.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "pc/test/fakesctptransport.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/virtualsocketserver.h"

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;
using ::testing::Values;

class FakePeerConnectionFactory
    : public rtc::RefCountedObject<PeerConnectionFactory> {
 public:
  FakePeerConnectionFactory()
      : rtc::RefCountedObject<PeerConnectionFactory>(
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::MakeUnique<cricket::FakeMediaEngine>(),
            CreateCallFactory(),
            nullptr) {}

  std::unique_ptr<cricket::SctpTransportInternalFactory>
  CreateSctpTransportInternalFactory() {
    auto factory = rtc::MakeUnique<FakeSctpTransportFactory>();
    last_fake_sctp_transport_factory_ = factory.get();
    return factory;
  }

  FakeSctpTransportFactory* last_fake_sctp_transport_factory_ = nullptr;
};

class PeerConnectionWrapperForDataChannelUnitTest
    : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

  FakeSctpTransportFactory* sctp_transport_factory() {
    return sctp_transport_factory_;
  }

  void set_sctp_transport_factory(
      FakeSctpTransportFactory* sctp_transport_factory) {
    sctp_transport_factory_ = sctp_transport_factory;
  }

  rtc::Optional<std::string> sctp_content_name() {
    return GetInternalPeerConnection()->sctp_content_name();
  }

  rtc::Optional<std::string> sctp_transport_name() {
    return GetInternalPeerConnection()->sctp_transport_name();
  }

  PeerConnection* GetInternalPeerConnection() {
    auto* pci = reinterpret_cast<
        PeerConnectionProxyWithInternal<PeerConnectionInterface>*>(pc());
    return reinterpret_cast<PeerConnection*>(pci->internal());
  }

 private:
  FakeSctpTransportFactory* sctp_transport_factory_ = nullptr;
};

class PeerConnectionDataChannelUnitTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForDataChannelUnitTest>
      WrapperPtr;

  PeerConnectionDataChannelUnitTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
#ifdef WEBRTC_ANDROID
    InitializeAndroidObjects();
#endif
  }

  WrapperPtr CreatePeerConnection() {
    return CreatePeerConnection(RTCConfiguration());
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    return CreatePeerConnection(config,
                                PeerConnectionFactoryInterface::Options());
  }

  WrapperPtr CreatePeerConnection(
      const RTCConfiguration& config,
      const PeerConnectionFactoryInterface::Options factory_options) {
    rtc::scoped_refptr<FakePeerConnectionFactory> pc_factory(
        new FakePeerConnectionFactory());
    pc_factory->SetOptions(factory_options);
    RTC_CHECK(pc_factory->Initialize());
    auto observer = rtc::MakeUnique<MockPeerConnectionObserver>();
    auto pc = pc_factory->CreatePeerConnection(config, nullptr, nullptr,
                                               observer.get());
    if (!pc) {
      return nullptr;
    }

    auto wrapper = rtc::MakeUnique<PeerConnectionWrapperForDataChannelUnitTest>(
        pc_factory, pc, std::move(observer));
    RTC_DCHECK(pc_factory->last_fake_sctp_transport_factory_);
    wrapper->set_sctp_transport_factory(
        pc_factory->last_fake_sctp_transport_factory_);
    return wrapper;
  }

  // Accepts the same arguments as CreatePeerConnection and adds a default data
  // channel.
  template <typename... Args>
  WrapperPtr CreatePeerConnectionWithDataChannel(Args&&... args) {
    auto wrapper = CreatePeerConnection(std::forward<Args>(args)...);
    if (!wrapper) {
      return nullptr;
    }
    EXPECT_TRUE(wrapper->pc()->CreateDataChannel("dc", nullptr));
    return wrapper;
  }

  // Changes the SCTP data channel port on the given session description.
  void ChangeSctpPortOnDescription(cricket::SessionDescription* desc,
                                   int port) {
    cricket::DataCodec sctp_codec(cricket::kGoogleSctpDataCodecPlType,
                                  cricket::kGoogleSctpDataCodecName);
    sctp_codec.SetParam(cricket::kCodecParamPort, port);

    auto* data_content = cricket::GetFirstDataContent(desc);
    RTC_DCHECK(data_content);
    auto* data_desc = static_cast<cricket::DataContentDescription*>(
        data_content->description);
    data_desc->set_codecs({sctp_codec});
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
};

TEST_F(PeerConnectionDataChannelUnitTest,
       NoSctpTransportCreatedIfRtpDataChannelEnabled) {
  RTCConfiguration config;
  config.enable_rtp_data_channel = true;
  auto caller = CreatePeerConnectionWithDataChannel(config);

  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));
  EXPECT_FALSE(caller->sctp_transport_factory()->last_fake_sctp_transport());
}

TEST_F(PeerConnectionDataChannelUnitTest,
       RtpDataChannelCreatedEvenIfSctpAvailable) {
  RTCConfiguration config;
  config.enable_rtp_data_channel = true;
  PeerConnectionFactoryInterface::Options options;
  options.disable_sctp_data_channels = false;
  auto caller = CreatePeerConnectionWithDataChannel(config, options);

  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));
  EXPECT_FALSE(caller->sctp_transport_factory()->last_fake_sctp_transport());
}

TEST_F(PeerConnectionDataChannelUnitTest,
       SctpContentAndTransportNameSetCorrectly) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  // Initially these fields should be empty.
  EXPECT_FALSE(caller->sctp_content_name());
  EXPECT_FALSE(caller->sctp_transport_name());

  // Create offer with audio/video/data.
  // Default bundle policy is "balanced", so data should be using its own
  // transport.
  caller->AddAudioVideoStream("s", "a", "v");
  caller->pc()->CreateDataChannel("dc", nullptr);
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_TRUE(caller->sctp_content_name());
  EXPECT_EQ(cricket::CN_DATA, *caller->sctp_content_name());
  ASSERT_TRUE(caller->sctp_transport_name());
  EXPECT_EQ(cricket::CN_DATA, *caller->sctp_transport_name());

  // Create answer that finishes BUNDLE negotiation, which means everything
  // should be bundled on the first transport (audio).
  RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  ASSERT_TRUE(caller->sctp_content_name());
  EXPECT_EQ(cricket::CN_DATA, *caller->sctp_content_name());
  ASSERT_TRUE(caller->sctp_transport_name());
  EXPECT_EQ(cricket::CN_AUDIO, *caller->sctp_transport_name());
}

TEST_F(PeerConnectionDataChannelUnitTest,
       CreateOfferWithNoStreamsGivesNoDataSection) {
  auto caller = CreatePeerConnection();
  auto offer = caller->CreateOffer();

  EXPECT_FALSE(offer->description()->GetContentByName(cricket::CN_DATA));
  EXPECT_FALSE(offer->description()->GetTransportInfoByName(cricket::CN_DATA));
}

TEST_F(PeerConnectionDataChannelUnitTest,
       CreateAnswerWithSctpDataChannelIncludesDataSection) {
  auto caller = CreatePeerConnectionWithDataChannel();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);
  EXPECT_TRUE(answer->description()->GetContentByName(cricket::CN_DATA));
  EXPECT_TRUE(answer->description()->GetTransportInfoByName(cricket::CN_DATA));
}

// The follow parameterized test verifies that the create data channel API
// either succeeds or fails according to the options given to the
// PeerConnection. Additionally, the cases are repeated when applying a remote
// offer with a SCTP data channel, verifying that the underlying SCTP transport
// is either created or not created.

class PeerConnectionDataChannelOptionsUnitTest
    : public PeerConnectionDataChannelUnitTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string,
                     RTCConfiguration,
                     PeerConnectionFactoryInterface::Options,
                     bool>> {
 protected:
  PeerConnectionDataChannelOptionsUnitTest() {
    config_ = std::get<1>(GetParam());
    options_ = std::get<2>(GetParam());
    expect_created_ = std::get<3>(GetParam());
  }

  RTCConfiguration config_;
  PeerConnectionFactoryInterface::Options options_;
  bool expect_created_;
};

TEST_P(PeerConnectionDataChannelOptionsUnitTest, TryCreateDataChannelFromApi) {
  auto caller = CreatePeerConnection(config_, options_);

  bool api_succeeded =
      (caller->pc()->CreateDataChannel("dc", nullptr) != nullptr);
  EXPECT_EQ(expect_created_, api_succeeded);
}

TEST_P(PeerConnectionDataChannelOptionsUnitTest, TryCreateDataChannelFromSdp) {
  auto caller = CreatePeerConnectionWithDataChannel();
  auto callee = CreatePeerConnection(config_, options_);

  // If DTLS is disabled, setting the remote offer will fail but that case is
  // already covered by crypto tests so we don't verify it again.
  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());

  bool sctp_transport_created =
      (callee->sctp_transport_factory()->last_fake_sctp_transport() != nullptr);
  EXPECT_EQ(expect_created_, sctp_transport_created);
}

RTCConfiguration DtlsDisabledConfig() {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  return config;
}

PeerConnectionFactoryInterface::Options SctpDisabledOptions() {
  PeerConnectionFactoryInterface::Options options;
  options.disable_sctp_data_channels = true;
  return options;
}

INSTANTIATE_TEST_CASE_P(
    PeerConnectionDataChannelUnitTest,
    PeerConnectionDataChannelOptionsUnitTest,
    Values(std::make_tuple("DTLS disabled",
                           DtlsDisabledConfig(),
                           PeerConnectionFactoryInterface::Options(),
                           false),
           std::make_tuple("default config/options",
                           RTCConfiguration(),
                           PeerConnectionFactoryInterface::Options(),
                           true),
           std::make_tuple("SCTP disabled",
                           RTCConfiguration(),
                           SctpDisabledOptions(),
                           false)));

TEST_F(PeerConnectionDataChannelUnitTest,
       SctpPortPropagatedFromSdpToTransport) {
  constexpr int kNewSendPort = 9998;
  constexpr int kNewRecvPort = 7775;

  auto caller = CreatePeerConnectionWithDataChannel();
  auto callee = CreatePeerConnectionWithDataChannel();

  auto offer = caller->CreateOffer();
  ChangeSctpPortOnDescription(offer->description(), kNewSendPort);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  ChangeSctpPortOnDescription(answer->description(), kNewRecvPort);
  ASSERT_TRUE(callee->SetLocalDescription(std::move(answer)));

  auto* callee_transport =
      callee->sctp_transport_factory()->last_fake_sctp_transport();
  ASSERT_TRUE(callee_transport);
  EXPECT_EQ(kNewSendPort, callee_transport->remote_port());
  EXPECT_EQ(kNewRecvPort, callee_transport->local_port());
}

#ifdef HAVE_QUIC
TEST_F(PeerConnectionDataChannelUnitTest, TestNegotiateQuic) {
  RTCConfiguration config;
  config.enable_quic = true;
  auto caller = CreatePeerConnectionWithDataChannel(config);
  auto callee = CreatePeerConnection(config);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));
}
#endif  // HAVE_QUIC

}  // namespace webrtc
