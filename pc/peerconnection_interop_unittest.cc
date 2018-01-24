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
#include "media/engine/webrtcmediaengine.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "pc/mediasession.h"
#include "pc/peerconnectionfactory.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/sdputils.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/fakesctptransport.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/virtualsocketserver.h"
#include "test/gmock.h"

// This file contains tests that verify interoperability between either local
// PeerConnections with different setting and/or simulations of other
// PeerConnection implementations.

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using ::testing::Values;

class PeerConnectionFactoryForInteropTest : public PeerConnectionFactory {
 public:
  PeerConnectionFactoryForInteropTest()
      : PeerConnectionFactory(
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::WrapUnique(cricket::WebRtcMediaEngineFactory::Create(
                FakeAudioCaptureModule::Create(),
                CreateBuiltinAudioEncoderFactory(),
                CreateBuiltinAudioDecoderFactory(),
                nullptr,
                nullptr,
                nullptr,
                AudioProcessingBuilder().Create())),
            CreateCallFactory(),
            nullptr) {}

  std::unique_ptr<cricket::SctpTransportInternalFactory>
  CreateSctpTransportInternalFactory() {
    return rtc::MakeUnique<FakeSctpTransportFactory>();
  }
};

class PeerConnectionInteropTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapper> WrapperPtr;

  PeerConnectionInteropTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
#ifdef WEBRTC_ANDROID
    InitializeAndroidObjects();
#endif
  }

  WrapperPtr CreatePeerConnectionWithSemantics(SdpSemantics semantics) {
    RTCConfiguration config;
    config.sdp_semantics = semantics;
    return CreatePeerConnection(config);
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    rtc::scoped_refptr<PeerConnectionFactory> pc_factory(
        new rtc::RefCountedObject<PeerConnectionFactoryForInteropTest>());
    RTC_CHECK(pc_factory->Initialize());
    auto observer = rtc::MakeUnique<MockPeerConnectionObserver>();
    auto pc = pc_factory->CreatePeerConnection(config, nullptr, nullptr,
                                               observer.get());
    if (!pc) {
      return nullptr;
    }

    return rtc::MakeUnique<PeerConnectionWrapper>(pc_factory, pc,
                                                  std::move(observer));
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
};

// Test that a PeerConnection configured with Plan B semantics can interop with
// a PeerConnection configured with Unified Plan semantics when there is at most
// 1 audio track and 1 video track.

class PeerConnectionInteropBiTest
    : public PeerConnectionInteropTest,
      public ::testing::WithParamInterface<
          std::tuple<SdpSemantics, SdpSemantics>> {
 protected:
  PeerConnectionInteropBiTest() {
    caller_semantics_ = std::get<0>(GetParam());
    callee_semantics_ = std::get<1>(GetParam());
  }

  WrapperPtr CreateCaller() {
    return CreatePeerConnectionWithSemantics(caller_semantics_);
  }

  WrapperPtr CreateCallee() {
    return CreatePeerConnectionWithSemantics(callee_semantics_);
  }

  SdpSemantics caller_semantics_;
  SdpSemantics callee_semantics_;
};

TEST_P(PeerConnectionInteropBiTest, NoMediaLocalToNoMediaRemote) {
  auto caller = CreateCaller();
  auto callee = CreateCallee();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
}

TEST_P(PeerConnectionInteropBiTest, OneAudioLocalToNoMediaRemote) {
  auto caller = CreateCaller();
  caller->AddAudioTrack("audio");
  auto callee = CreateCallee();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
}

TEST_P(PeerConnectionInteropBiTest, OneAudioOneVideoToNoMediaRemote) {
  auto caller = CreateCaller();
  caller->AddVideoTrack("video");
  caller->AddAudioTrack("audio");
  auto callee = CreateCallee();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
}

TEST_P(PeerConnectionInteropBiTest, OneAudioLocalToOneVideoRemote) {
  auto caller = CreateCaller();
  caller->AddAudioTrack("audio");
  auto callee = CreateCallee();
  callee->AddVideoTrack("video");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
}

TEST_P(PeerConnectionInteropBiTest,
       OneAudioOneVideoLocalToOneAudioOneVideoRemote) {
  auto caller = CreateCaller();
  caller->AddAudioTrack("caller_audio");
  caller->AddVideoTrack("caller_video");
  auto callee = CreateCallee();
  callee->AddAudioTrack("callee_audio");
  callee->AddVideoTrack("callee_video");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
}

TEST_P(PeerConnectionInteropBiTest, ReverseRolesOneAudioLocalToOneVideoRemote) {
  auto caller = CreateCaller();
  caller->AddAudioTrack("audio");
  auto callee = CreateCaller();
  callee->AddVideoTrack("video");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  // Reverse roles.
  ASSERT_TRUE(callee->ExchangeOfferAnswerWith(caller.get()));
}

INSTANTIATE_TEST_CASE_P(
    PeerConnectionInteropTest,
    PeerConnectionInteropBiTest,
    Values(std::make_tuple(SdpSemantics::kPlanB, SdpSemantics::kUnifiedPlan),
           std::make_tuple(SdpSemantics::kUnifiedPlan, SdpSemantics::kPlanB)));

}  // namespace webrtc
