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
#include "pc/sdputils.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "pc/test/fakeaudiocapturemodule.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/virtualsocketserver.h"
#include "test/gmock.h"

namespace webrtc {

using cricket::MediaContentDescription;
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

// Tests for JSEP initial offer generation.

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

// Tests for JSEP SetLocalDescription with a local offer.

TEST_F(PeerConnectionMultiTrackTest, SetLocalEmptyOfferCreatesNoTransceivers) {
  auto caller = CreatePeerConnection();
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  EXPECT_THAT(caller->pc()->GetTransceivers(), ElementsAre());
  EXPECT_THAT(caller->pc()->GetSenders(), ElementsAre());
  EXPECT_THAT(caller->pc()->GetReceivers(), ElementsAre());
}

TEST_F(PeerConnectionMultiTrackTest, SetLocalOfferSetsTransceiverMid) {
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

// Tests for JSEP SetRemoteDescription with a remote offer.

// Test that setting a remote offer with sendrecv audio and video creates two
// transceivers, one for receiving audio and one for receiving video.
TEST_F(PeerConnectionMultiTrackTest, SetRemoteOfferCreatesTransceivers) {
  auto caller = CreatePeerConnection();
  auto caller_audio = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto caller_video = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO,
            transceivers[0]->receiver()->media_type());
  EXPECT_EQ(caller_audio->mid(), transceivers[0]->mid());
  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly, transceivers[0]->direction());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO,
            transceivers[1]->receiver()->media_type());
  EXPECT_EQ(caller_video->mid(), transceivers[1]->mid());
  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly, transceivers[1]->direction());
}

// Test that setting a remote offer with an audio track will reuse the
// transceiver created for a local audio track added by AddTrack.
TEST_F(PeerConnectionMultiTrackTest,
       SetRemoteOfferReusesTransceiverFromAddTrack) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto caller_audio = caller->pc()->GetTransceivers()[0];
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind,
            transceivers[0]->receiver()->track()->kind());
  EXPECT_EQ(caller_audio->mid(), transceivers[0]->mid());
}

// Test that setting a remote offer with an audio track marked sendonly will not
// reuse a transceiver created by AddTrack. JSEP only allows the transceiver to
// be reused if the offer direction is sendrecv or recvonly.
TEST_F(PeerConnectionMultiTrackTest,
       SetRemoteOfferDoesNotReuseTransceiverIfDirectionSendOnly) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto caller_audio = caller->pc()->GetTransceivers()[0];
  caller_audio->SetDirection(RtpTransceiverDirection::kSendOnly);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(rtc::nullopt, transceivers[0]->mid());
  EXPECT_EQ(caller_audio->mid(), transceivers[1]->mid());
}

// Test that setting a remote offer with an audio track will not reuse a
// transceiver added by AddTransceiver.
TEST_F(PeerConnectionMultiTrackTest,
       SetRemoteOfferDoesNotReuseTransceiverFromAddTransceiver) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  auto transceiver = callee->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(rtc::nullopt, transceivers[0]->mid());
  EXPECT_EQ(caller->pc()->GetTransceivers()[0]->mid(), transceivers[1]->mid());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind,
            transceivers[1]->receiver()->track()->kind());
}

// Test that setting a remote offer with an audio track will not reuse a
// transceiver created for a local video track added by AddTrack.
TEST_F(PeerConnectionMultiTrackTest,
       SetRemoteOfferDoesNotReuseTransceiverOfWrongType) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  auto video_sender = callee->AddVideoTrack("v");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(rtc::nullopt, transceivers[0]->mid());
  EXPECT_EQ(caller->pc()->GetTransceivers()[0]->mid(), transceivers[1]->mid());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind,
            transceivers[1]->receiver()->track()->kind());
}

// Test that setting a remote offer with an audio track will not reuse a
// stopped transceiver.
TEST_F(PeerConnectionMultiTrackTest,
       SetRemoteOfferDoesNotReuseStoppedTransceiver) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  callee->pc()->GetTransceivers()[0]->Stop();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(rtc::nullopt, transceivers[0]->mid());
  EXPECT_TRUE(transceivers[0]->stopped());
  EXPECT_EQ(caller->pc()->GetTransceivers()[0]->mid(), transceivers[1]->mid());
  EXPECT_FALSE(transceivers[1]->stopped());
}

TEST_F(PeerConnectionMultiTrackTest,
       SetRemoteOfferReusesTransceiversOfBothTypes) {
  auto caller = CreatePeerConnection();
  caller->AddVideoTrack("v");
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  callee->AddVideoTrack("v");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto caller_transceivers = caller->pc()->GetTransceivers();
  auto callee_transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, callee_transceivers.size());
  EXPECT_EQ(caller_transceivers[0]->mid(), callee_transceivers[1]->mid());
  EXPECT_EQ(caller_transceivers[1]->mid(), callee_transceivers[0]->mid());
}

// Tests for JSEP initial CreateAnswer.

// Test that the answer to a remote offer creates media sections for each
// offered media in the same order and with the same mids.
TEST_F(PeerConnectionMultiTrackTest, CreateAnswerHasSameMidsAsOffer) {
  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto second_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto third_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  const auto& contents = answer->description()->contents();
  ASSERT_EQ(3u, contents.size());
  EXPECT_EQ(
      cricket::MEDIA_TYPE_VIDEO,
      static_cast<MediaContentDescription*>(contents[0].description)->type());
  EXPECT_EQ(*first_transceiver->mid(), contents[0].name);
  EXPECT_EQ(
      cricket::MEDIA_TYPE_AUDIO,
      static_cast<MediaContentDescription*>(contents[1].description)->type());
  EXPECT_EQ(*second_transceiver->mid(), contents[1].name);
  EXPECT_EQ(
      cricket::MEDIA_TYPE_VIDEO,
      static_cast<MediaContentDescription*>(contents[2].description)->type());
  EXPECT_EQ(*third_transceiver->mid(), contents[2].name);
}

// Test that an answering media section is marked as rejected if the underlying
// transceiver has been stopped.
TEST_F(PeerConnectionMultiTrackTest, CreateAnswerRejectsStoppedTransceiver) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  callee->pc()->GetTransceivers()[0]->Stop();

  auto answer = callee->CreateAnswer();
  const auto& contents = answer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_TRUE(contents[0].rejected);
}

TEST_F(PeerConnectionMultiTrackTest, CreateAnswerNegotiatesDirection) {
  auto caller = CreatePeerConnection();
  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  const auto& contents = answer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly,
            static_cast<MediaContentDescription*>(contents[0].description)
                ->direction());
}

// Tests for JSEP SetLocalDescription with a local answer.
// Note that these test only the additional behaviors not covered by
// SetLocalDescription with a local offer.

// Test that SetLocalDescription with an answer sets the current_direction
// property of the transceivers mentioned in the session description.
TEST_F(PeerConnectionMultiTrackTest, SetLocalAnswerUpdatesCurrentDirection) {
  auto caller = CreatePeerConnection();
  auto caller_audio = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  caller_audio->SetDirection(RtpTransceiverDirection::kRecvOnly);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  // Since the offer was recvonly and the transceiver direction is sendrecv,
  // the negotiated direction will be sendonly.
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly,
            transceivers[0]->current_direction());
}

// Tests for JSEP SetRemoteDescription with a remote answer.
// Note that these test only the additional behaviors not covered by
// SetRemoteDescription with a remote offer.

TEST_F(PeerConnectionMultiTrackTest, SetRemoteAnswerUpdatesCurrentDirection) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  auto callee_audio = callee->pc()->GetTransceivers()[0];
  callee_audio->SetDirection(RtpTransceiverDirection::kSendOnly);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  // Since the remote transceiver was set to sendonly, the negotiated direction
  // in the answer would be sendonly which we apply as recvonly to the local
  // transceiver.
  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly,
            transceivers[0]->current_direction());
}

// Tests for multiple round trips.

TEST_F(PeerConnectionMultiTrackTest, blah) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  callee->pc()->GetTransceivers()[0]->SetDirection(
      RtpTransceiverDirection::kInactive);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  EXPECT_FALSE(caller->pc()->GetTransceivers()[0]->stopped());
  EXPECT_FALSE(callee->pc()->GetTransceivers()[0]->stopped());
}

// Test that if a transceiver had been associated and later stopped, then a
// media section is still generated for it and the media section is marked as
// rejected.
TEST_F(PeerConnectionMultiTrackTest,
       ReOfferMediaSectionForAssociatedStoppedTransceiverIsRejected) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  ASSERT_TRUE(transceiver->mid());
  transceiver->Stop();

  auto reoffer = caller->CreateOffer();
  const auto& contents = reoffer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_TRUE(contents[0].rejected);
}

// Test that stopping an associated transceiver on the caller side will stop the
// corresponding transceiver on the remote side when the remote offer is
// applied.
TEST_F(PeerConnectionMultiTrackTest,
       StoppingTransceiverInOfferStopsTransceiverOnRemoteSide) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  transceiver->Stop();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_TRUE(callee->pc()->GetTransceivers()[0]->stopped());
}

TEST_F(PeerConnectionMultiTrackTest,
       CreateOfferDoesNotRecycleMediaSectionIfFirstStopped) {
  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto second_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  first_transceiver->Stop();

  auto reoffer = caller->CreateOffer();
  const auto& contents = reoffer->description()->contents();
  ASSERT_EQ(2u, contents.size());
  EXPECT_TRUE(contents[0].rejected);
  EXPECT_FALSE(contents[1].rejected);
}

//
TEST_F(PeerConnectionMultiTrackTest, RecycledMediaSectionUpdatesTransceivers) {
  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  std::string first_mid = *first_transceiver->mid();
  first_transceiver->Stop();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto second_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  auto offer = caller->CreateOffer();
  const auto& offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());
  EXPECT_FALSE(offer_contents[0].rejected);
  std::string second_mid = offer_contents[0].name;
  EXPECT_NE(first_mid, second_mid);

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));

  EXPECT_EQ(rtc::nullopt, first_transceiver->mid());
  EXPECT_EQ(second_mid, second_transceiver->mid());

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto callee_transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, callee_transceivers.size());
  EXPECT_EQ(rtc::nullopt, callee_transceivers[0]->mid());
  EXPECT_EQ(second_mid, callee_transceivers[1]->mid());

  auto answer = callee->CreateAnswer();
  const auto& answer_contents = answer->description()->contents();
  ASSERT_EQ(1u, answer_contents.size());
  EXPECT_FALSE(answer_contents[0].rejected);
  EXPECT_EQ(second_mid, answer_contents[0].name);

  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));

  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}

TEST_F(PeerConnectionMultiTrackTest,
       RecycleAudioMediaSectionAsVideoMediaSection) {
  auto caller = CreatePeerConnection();
  auto audio_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  audio_transceiver->Stop();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto video_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);

  auto offer = caller->CreateOffer();
  const auto& offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO,
            static_cast<MediaContentDescription*>(offer_contents[0].description)
                ->type());

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));

  EXPECT_FALSE(audio_transceiver->mid());
  EXPECT_TRUE(video_transceiver->mid());
}

TEST_F(PeerConnectionMultiTrackTest,
       RecycleVideoMediaSectionAsAudioMediaSection) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  transceiver->Stop();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  auto offer = caller->CreateOffer();
  const auto& contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(
      cricket::MEDIA_TYPE_AUDIO,
      static_cast<MediaContentDescription*>(contents[0].description)->type());
}

TEST_F(PeerConnectionMultiTrackTest, OfferAnswerWithOneAudioOneVideo) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  caller->AddVideoTrack("v");
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto caller_transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, caller_transceivers.size());

  auto callee_transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, callee_transceivers.size());
}

}  // namespace webrtc
