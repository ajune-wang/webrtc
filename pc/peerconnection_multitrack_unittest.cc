/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "pc/mediasession.h"
#include "pc/peerconnectionwrapper.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "pc/test/fakeaudiocapturemodule.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/virtualsocketserver.h"
#include "test/gmock.h"

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;
using ::testing::Values;
using ::testing::Combine;
using ::testing::ElementsAre;

class PeerConnectionMultiTrackTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapper> WrapperPtr;

  PeerConnectionMultiTrackTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
#ifdef WEBRTC_ANDROID
    InitializeAndroidObjects();
#endif
    pc_factory_ = CreatePeerConnectionFactory(
        rtc::Thread::Current(), rtc::Thread::Current(), rtc::Thread::Current(),
        FakeAudioCaptureModule::Create(), CreateBuiltinAudioEncoderFactory(),
        CreateBuiltinAudioDecoderFactory(), nullptr, nullptr);
  }

  WrapperPtr CreatePeerConnection() {
    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    return CreatePeerConnection(config);
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    auto observer = rtc::MakeUnique<MockPeerConnectionObserver>();
    auto pc = pc_factory_->CreatePeerConnection(config, nullptr, nullptr,
                                                observer.get());
    if (!pc) {
      return nullptr;
    }

    return rtc::MakeUnique<PeerConnectionWrapper>(pc_factory_, pc,
                                                  std::move(observer));
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
};

// Test that an offer created by a PeerConnection with no transceivers generates
// no media sections.
TEST_F(PeerConnectionMultiTrackTest, EmptyInitialOffer) {
  auto caller = CreatePeerConnection();
  auto offer = caller->CreateOffer();
  EXPECT_EQ(0u, offer->description()->contents().size());
}

// Test that an initial offer with one audio track generates one audio media
// section.
TEST_F(PeerConnectionMultiTrackTest, AudioOnlyInitialOffer) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto offer = caller->CreateOffer();

  ASSERT_EQ(1u, offer->description()->contents().size());
  const cricket::ContentInfo& content_info =
      offer->description()->contents()[0];
  auto* media_description =
      static_cast<cricket::MediaContentDescription*>(content_info.description);
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, media_description->type());
}

// Test than an initial offer with one video track generates one video media
// section
TEST_F(PeerConnectionMultiTrackTest, VideoOnlyInitialOffer) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto offer = caller->CreateOffer();

  ASSERT_EQ(1u, offer->description()->contents().size());
  const cricket::ContentInfo& content_info =
      offer->description()->contents()[0];
  auto* media_description =
      static_cast<cricket::MediaContentDescription*>(content_info.description);
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, media_description->type());
}

// Test that multiple media sections in the initial offer are ordered in the
// order the transceivers were added to the PeerConnection. This is required by
// JSEP section 5.2.1.
TEST_F(PeerConnectionMultiTrackTest,
       MediaSectionsInInitialOfferOrderedCorrectly) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  auto offer = caller->CreateOffer();

  const cricket::ContentInfos& contents = offer->description()->contents();
  ASSERT_EQ(3u, contents.size());

  auto* media_description1 =
      static_cast<cricket::MediaContentDescription*>(contents[0].description);
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, media_description1->type());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv,
            media_description1->direction());

  auto* media_description2 =
      static_cast<cricket::MediaContentDescription*>(contents[1].description);
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, media_description2->type());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv,
            media_description2->direction());

  auto* media_description3 =
      static_cast<cricket::MediaContentDescription*>(contents[2].description);
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, media_description3->type());
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly,
            media_description3->direction());
}

// Test that media sections in the initial offer have different mids.
TEST_F(PeerConnectionMultiTrackTest,
       MediaSectionsInInitialOfferHaveDifferentMids) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto offer = caller->CreateOffer();

  std::string sdp;
  offer->ToString(&sdp);
  RTC_LOG(LS_INFO) << sdp;

  const cricket::ContentInfos& contents = offer->description()->contents();
  ASSERT_EQ(2u, contents.size());
  EXPECT_NE(contents[0].name, contents[1].name);
}

TEST_F(PeerConnectionMultiTrackTest,
       StoppedTransceiverHasNoMediaSectionInInitialOffer) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  transceiver->Stop();

  auto offer = caller->CreateOffer();
  EXPECT_EQ(0u, offer->description()->contents().size());
}

TEST_F(PeerConnectionMultiTrackTest,
       SetLocalDescriptionWithEmptyOfferCreatesNoTransceivers) {
  auto caller = CreatePeerConnection();
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  EXPECT_THAT(caller->pc()->GetTransceivers(), ElementsAre());
  EXPECT_THAT(caller->pc()->GetSenders(), ElementsAre());
  EXPECT_THAT(caller->pc()->GetReceivers(), ElementsAre());
}

TEST_F(PeerConnectionMultiTrackTest, SetLocalDescriptionSetsTransceiverMid) {
  auto caller = CreatePeerConnection();
  auto audio_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto video_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);

  auto offer = caller->CreateOffer();
  std::string audio_mid = offer->description()->contents()[0].name;
  std::string video_mid = offer->description()->contents()[1].name;

  ASSERT_TRUE(caller->SetLocalDescription(std::move(offer)));

  EXPECT_EQ(audio_mid, audio_transceiver->mid());
  EXPECT_EQ(video_mid, video_transceiver->mid());
}

}  // namespace webrtc
