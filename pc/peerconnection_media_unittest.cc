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

#include "call/callfactoryinterface.h"
#include "logging/rtc_event_log/rtc_event_log_factory.h"
#include "media/base/fakemediaengine.h"
#include "p2p/base/fakeportallocator.h"
#include "pc/mediasession.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/sdputils.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/virtualsocketserver.h"
#include "test/gmock.h"

namespace {

using RTCConfiguration = webrtc::PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions =
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions;
using ::testing::Values;
using ::testing::Combine;
using ::testing::ElementsAre;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionFactoryInterface;
using webrtc::SessionDescriptionInterface;

class PeerConnectionWrapperForMediaUnitTest
    : public webrtc::PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

  cricket::FakeMediaEngine* media_engine() { return media_engine_; }
  void set_media_engine(cricket::FakeMediaEngine* media_engine) {
    media_engine_ = media_engine;
  }

 private:
  cricket::FakeMediaEngine* media_engine_;
};

class PeerConnectionMediaUnitTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForMediaUnitTest> WrapperPtr;

  PeerConnectionMediaUnitTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
#ifdef WEBRTC_ANDROID
    webrtc::InitializeAndroidObjects();
#endif
  }

  WrapperPtr CreatePeerConnection() {
    RTCConfiguration default_config;
    return CreatePeerConnection(default_config);
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    auto media_engine = rtc::MakeUnique<cricket::FakeMediaEngine>();
    auto* media_engine_ptr = media_engine.get();
    auto pc_factory = webrtc::CreateModularPeerConnectionFactory(
        rtc::Thread::Current(), rtc::Thread::Current(), rtc::Thread::Current(),
        std::move(media_engine), webrtc::CreateCallFactory(),
        webrtc::CreateRtcEventLogFactory());

    auto fake_port_allocator = rtc::MakeUnique<cricket::FakePortAllocator>(
        rtc::Thread::Current(), nullptr);
    auto observer = rtc::MakeUnique<webrtc::MockPeerConnectionObserver>();
    auto pc = pc_factory->CreatePeerConnection(
        config, std::move(fake_port_allocator), nullptr, observer.get());
    if (!pc) {
      return nullptr;
    }

    auto wrapper = rtc::MakeUnique<PeerConnectionWrapperForMediaUnitTest>(
        pc_factory, pc, std::move(observer));
    wrapper->set_media_engine(media_engine_ptr);
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

  const cricket::MediaContentDescription* GetMediaContent(
      const SessionDescriptionInterface* sdesc,
      const std::string& mid) {
    const auto* content_desc =
        sdesc->description()->GetContentDescriptionByName(mid);
    return static_cast<const cricket::MediaContentDescription*>(content_desc);
  }

  cricket::MediaContentDirection GetMediaContentDirection(
      const SessionDescriptionInterface* sdesc,
      const std::string& mid) {
    const auto* media_desc = GetMediaContent(sdesc, mid);
    if (media_desc) {
      return media_desc->direction();
    } else {
      return cricket::MD_INACTIVE;
    }
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
};

TEST_F(PeerConnectionMediaUnitTest, SetLocalOfferTwiceWorks) {
  auto caller = CreatePeerConnection();

  EXPECT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));
  EXPECT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));
}

TEST_F(PeerConnectionMediaUnitTest, SetRemoteOfferTwiceWorks) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
}

TEST_F(PeerConnectionMediaUnitTest, FailToSetNullLocalDescription) {
  auto caller = CreatePeerConnection();
  std::string error;
  ASSERT_FALSE(caller->SetLocalDescription(nullptr, &error));
  EXPECT_EQ("SessionDescription is NULL.", error);
}

TEST_F(PeerConnectionMediaUnitTest, FailToSetNullRemoteDescription) {
  auto caller = CreatePeerConnection();
  std::string error;
  ASSERT_FALSE(caller->SetRemoteDescription(nullptr, &error));
  EXPECT_EQ("SessionDescription is NULL.", error);
}

TEST_F(PeerConnectionMediaUnitTest, FailToCreateAnswerWithNoRemoteDescription) {
  auto callee = CreatePeerConnection();
  std::string error;
  ASSERT_FALSE(callee->CreateAnswer(RTCOfferAnswerOptions(), &error));
  EXPECT_EQ("CreateAnswer called without remote offer.", error);
}

TEST_F(PeerConnectionMediaUnitTest,
       FailToCreateAnswerWithAnswerAsRemoteDescription) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  std::string error;
  ASSERT_FALSE(caller->CreateAnswer(RTCOfferAnswerOptions(), &error));
  EXPECT_EQ("CreateAnswer called without remote offer.", error);
}

TEST_F(PeerConnectionMediaUnitTest, FailToSetRemoteOfferAfterLocalOfferSet) {
  auto caller = CreatePeerConnection();
  auto offer = caller->CreateOfferAndSetAsLocal();

  std::string error;
  ASSERT_FALSE(caller->SetRemoteDescription(std::move(offer), &error));
  EXPECT_EQ(
      "Failed to set remote offer sdp: Called in wrong state: STATE_SENTOFFER",
      error);
}

TEST_F(PeerConnectionMediaUnitTest, FailToSetLocalOfferAfterRemoteOfferSet) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  callee->SetRemoteDescription(caller->CreateOffer());
  auto offer = caller->CreateOffer();

  std::string error;
  ASSERT_FALSE(callee->SetLocalDescription(std::move(offer), &error));
  EXPECT_EQ(
      "Failed to set local offer sdp: Called in wrong state: "
      "STATE_RECEIVEDOFFER",
      error);
}

TEST_F(PeerConnectionMediaUnitTest, FailToSetRemoteAnswerWithoutOffer) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();

  std::string error;
  ASSERT_FALSE(caller->SetRemoteDescription(std::move(answer), &error));
  EXPECT_EQ(
      "Failed to set remote answer sdp: Called in wrong state: STATE_INIT",
      error);
}

TEST_F(PeerConnectionMediaUnitTest, FailToSetLocalAnswerWithoutOffer) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();

  std::string error;
  ASSERT_FALSE(caller->SetLocalDescription(std::move(answer), &error));
  EXPECT_EQ("Failed to set local answer sdp: Called in wrong state: STATE_INIT",
            error);
}

TEST_F(PeerConnectionMediaUnitTest,
       FailToSetRemoteDescriptionIfCreateMediaChannelFails) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();
  callee->media_engine()->set_fail_create_channel(true);

  std::string error;
  ASSERT_FALSE(callee->SetRemoteDescription(caller->CreateOffer(), &error));
  EXPECT_EQ("Failed to set remote offer sdp: Failed to create channels.",
            error);
}

TEST_F(PeerConnectionMediaUnitTest,
       FailToSetLocalDescriptionIfCreateMediaChannelFails) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  caller->media_engine()->set_fail_create_channel(true);

  std::string error;
  ASSERT_FALSE(caller->SetLocalDescription(caller->CreateOffer(), &error));
  EXPECT_EQ("Failed to set local offer sdp: Failed to create channels.", error);
}

// According to https://tools.ietf.org/html/rfc3264#section-8, the session id
// stays the same but the version must be incremented if a later, different
// session description is generated. These two tests verify that is the case for
// both offers and answers.
TEST_F(PeerConnectionMediaUnitTest,
       SessionVersionIncrementedInSubsequentDifferentOffer) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto original_offer = caller->CreateOfferAndSetAsLocal();
  const std::string original_id = original_offer->session_id();
  const std::string original_version = original_offer->session_version();

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(original_offer)));
  ASSERT_TRUE(caller->SetRemoteDescription(callee->CreateAnswer()));

  // Add streams to get a different offer.
  caller->AddAudioVideoStream("s", "a", "v");

  auto later_offer = caller->CreateOffer();

  EXPECT_EQ(original_id, later_offer->session_id());
  EXPECT_LT(rtc::FromString<uint64_t>(original_version),
            rtc::FromString<uint64_t>(later_offer->session_version()));
}
TEST_F(PeerConnectionMediaUnitTest,
       SessionVersionIncrementedInSubsequentDifferentAnswer) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto original_answer = callee->CreateAnswerAndSetAsLocal();
  const std::string original_id = original_answer->session_id();
  const std::string original_version = original_answer->session_version();

  // Add streams to get a different answer.
  callee->AddAudioVideoStream("s", "a", "v");

  auto later_answer = callee->CreateAnswer();

  EXPECT_EQ(original_id, later_answer->session_id());
  EXPECT_LT(rtc::FromString<uint64_t>(original_version),
            rtc::FromString<uint64_t>(later_answer->session_version()));
}

std::vector<std::string> GetIds(
    const std::vector<cricket::StreamParams>& streams) {
  std::vector<std::string> ids;
  for (const auto& stream : streams) {
    ids.push_back(stream.id);
  }
  return ids;
}

// Test that exchanging an offer and answer with each side having an audio and
// video stream creates the appropriate send/recv streams in the underlying
// media engine on both sides.
TEST_F(PeerConnectionMediaUnitTest,
       AudioVideoOfferAnswerCreateSendRecvStreams) {
  const std::string kCallerStream = "caller_s";
  const std::string kCallerAudioTrack = "caller_a";
  const std::string kCallerVideoTrack = "caller_v";
  const std::string kCalleeStream = "callee_s";
  const std::string kCalleeAudioTrack = "callee_a";
  const std::string kCalleeVideoTrack = "callee_v";

  auto caller = CreatePeerConnection();
  caller->AddAudioVideoStream(kCallerStream, kCallerAudioTrack,
                              kCallerVideoTrack);

  auto callee = CreatePeerConnection();
  callee->AddAudioVideoStream(kCalleeStream, kCalleeAudioTrack,
                              kCalleeVideoTrack);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto* caller_voice = caller->media_engine()->GetVoiceChannel(0);
  EXPECT_THAT(GetIds(caller_voice->recv_streams()),
              ElementsAre(kCalleeAudioTrack));
  EXPECT_THAT(GetIds(caller_voice->send_streams()),
              ElementsAre(kCallerAudioTrack));

  auto* caller_video = caller->media_engine()->GetVideoChannel(0);
  EXPECT_THAT(GetIds(caller_video->recv_streams()),
              ElementsAre(kCalleeVideoTrack));
  EXPECT_THAT(GetIds(caller_video->send_streams()),
              ElementsAre(kCallerVideoTrack));

  auto* callee_voice = callee->media_engine()->GetVoiceChannel(0);
  EXPECT_THAT(GetIds(callee_voice->recv_streams()),
              ElementsAre(kCallerAudioTrack));
  EXPECT_THAT(GetIds(callee_voice->send_streams()),
              ElementsAre(kCalleeAudioTrack));

  auto* callee_video = callee->media_engine()->GetVideoChannel(0);
  EXPECT_THAT(GetIds(callee_video->recv_streams()),
              ElementsAre(kCallerVideoTrack));
  EXPECT_THAT(GetIds(callee_video->send_streams()),
              ElementsAre(kCalleeVideoTrack));
}

// Test that removing streams from the offer causes the underlying receive
// streams on the recipient to be removed.
TEST_F(PeerConnectionMediaUnitTest, EmptyOfferRemovesRecvStreams) {
  auto caller = CreatePeerConnection();
  auto caller_stream = caller->AddAudioVideoStream("s1", "a1", "v1");
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  // Remove send stream from caller.
  caller->pc()->RemoveStream(caller_stream);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  EXPECT_EQ(1u, callee_voice->send_streams().size());
  EXPECT_EQ(0u, callee_voice->recv_streams().size());

  auto callee_video = callee->media_engine()->GetVideoChannel(0);
  EXPECT_EQ(1u, callee_video->send_streams().size());
  EXPECT_EQ(0u, callee_video->recv_streams().size());
}

// Test that removing streams from the answer removes the underlying send
// streams when applied locally.
TEST_F(PeerConnectionMediaUnitTest, EmptyAnswerRemovesSendStreams) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();
  auto callee_stream = callee->AddAudioVideoStream("s2", "a2", "v2");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  // Remove send stream from callee.
  callee->pc()->RemoveStream(callee_stream);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  EXPECT_EQ(0u, callee_voice->send_streams().size());
  EXPECT_EQ(1u, callee_voice->recv_streams().size());

  auto callee_video = callee->media_engine()->GetVideoChannel(0);
  EXPECT_EQ(0u, callee_video->send_streams().size());
  EXPECT_EQ(1u, callee_video->recv_streams().size());
}

// Test that a new stream in the offer causes a new stream to be added to the
// media engine on the recipient side.
TEST_F(PeerConnectionMediaUnitTest, NewStreamInOfferAddsRecvStreams) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  // Add second stream to caller.
  caller->AddAudioVideoStream("s2", "a2", "v2");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  EXPECT_EQ(2u, callee_voice->recv_streams().size());
  auto callee_video = callee->media_engine()->GetVideoChannel(0);
  EXPECT_EQ(2u, callee_video->recv_streams().size());
}

// A PeerConnection with no local streams and no explicit answer constraints
// should not reject any offered media sections.
TEST_F(PeerConnectionMediaUnitTest,
       CreateAnswerWithNoStreamsAndDefaultConstraintsDoesNotReject) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  auto answer = callee->CreateAnswer();

  const auto* audio_content =
      cricket::GetFirstAudioContent(answer->description());
  ASSERT_TRUE(audio_content);
  EXPECT_FALSE(audio_content->rejected);

  const auto* video_content =
      cricket::GetFirstVideoContent(answer->description());
  ASSERT_TRUE(video_content);
  EXPECT_FALSE(video_content->rejected);
}

// The following parameterized test verifies that CreateOffer/CreateAnswer
// generate the appropriate media sections depending on which streams are
// created and which constraints are given when creating the offer/answer. The
// parameter specifies the expected

class PeerConnectionMediaConstraintsUnitTest
    : public PeerConnectionMediaUnitTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<cricket::MediaContentDirection,
                           cricket::MediaContentDirection>> {
 protected:
  PeerConnectionMediaConstraintsUnitTest() {
    audio_dir_ = ::testing::get<0>(GetParam());
    video_dir_ = ::testing::get<1>(GetParam());
  }

  bool DirHasSend(cricket::MediaContentDirection dir) {
    return dir == cricket::MD_SENDONLY || dir == cricket::MD_SENDRECV;
  }

  bool DirHasRecv(cricket::MediaContentDirection dir) {
    return dir == cricket::MD_RECVONLY || dir == cricket::MD_SENDRECV;
  }

  bool ExpectRejected(cricket::MediaContentDirection dir) {
    return dir == cricket::MD_SENDONLY;
  }

  WrapperPtr CreatePeerConnectionWithStreams() {
    auto wrapper = CreatePeerConnection();
    if (DirHasSend(audio_dir_)) {
      wrapper->AddAudioStream("audio_stream", "audio");
    }
    if (DirHasSend(video_dir_)) {
      wrapper->AddVideoStream("video_stream", "video");
    }
    return wrapper;
  }

  RTCOfferAnswerOptions GetOptionsWithConstraints() {
    RTCOfferAnswerOptions options;
    options.offer_to_receive_audio =
        (DirHasRecv(audio_dir_)
             ? RTCOfferAnswerOptions::kOfferToReceiveMediaTrue
             : 0);
    options.offer_to_receive_video =
        (DirHasRecv(video_dir_)
             ? RTCOfferAnswerOptions::kOfferToReceiveMediaTrue
             : 0);
    return options;
  }

  cricket::MediaContentDirection audio_dir_;
  cricket::MediaContentDirection video_dir_;
};

// Test that CreateOffer generates an offer with the correct media content
// direction for audio and video. Adding a local audio/video stream should cause
// the direction to include send. Specifying an offer to receive in the offer
// options should cause the direction to include receive.
TEST_P(PeerConnectionMediaConstraintsUnitTest,
       CreateOfferGeneratesMediaSectionsWithCorrectDirection) {
  auto caller = CreatePeerConnectionWithStreams();
  auto offer = caller->CreateOffer(GetOptionsWithConstraints());
  ASSERT_TRUE(offer);

  EXPECT_EQ(audio_dir_,
            GetMediaContentDirection(offer.get(), cricket::CN_AUDIO));
  EXPECT_EQ(video_dir_,
            GetMediaContentDirection(offer.get(), cricket::CN_VIDEO));
}

// Test that CreateAnswer generates an answer with the correct media sections
// and appropriate rejection status in response to an offer with audio and video
// streams. The callee does not have any local streams, so it should accept the
// offered streams as receive-only as long as the answer options include offer
// to receive the media type.
TEST_P(PeerConnectionMediaConstraintsUnitTest,
       CreateAnswerGeneratesMediaSectionsWithCorrectRejection) {
  auto caller = CreatePeerConnectionWithStreams();
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  auto answer = callee->CreateAnswer(GetOptionsWithConstraints());
  ASSERT_TRUE(answer);

  if (DirHasSend(audio_dir_)) {
    const auto* audio_content =
        cricket::GetFirstAudioContent(answer->description());
    ASSERT_TRUE(audio_content);
    EXPECT_EQ(ExpectRejected(audio_dir_), audio_content->rejected);
  }

  if (DirHasSend(video_dir_)) {
    const auto* video_content =
        cricket::GetFirstVideoContent(answer->description());
    ASSERT_TRUE(video_content);
    EXPECT_EQ(ExpectRejected(video_dir_), video_content->rejected);
  }
}

INSTANTIATE_TEST_CASE_P(PeerConnectionMediaUnitTest,
                        PeerConnectionMediaConstraintsUnitTest,
                        Combine(Values(cricket::MD_INACTIVE,
                                       cricket::MD_SENDONLY,
                                       cricket::MD_RECVONLY,
                                       cricket::MD_SENDRECV),
                                Values(cricket::MD_INACTIVE,
                                       cricket::MD_SENDONLY,
                                       cricket::MD_RECVONLY,
                                       cricket::MD_SENDRECV)));

void AddComfortNoiseCodecsToSend(cricket::FakeMediaEngine* media_engine) {
  const cricket::AudioCodec kComfortNoiseCodec8k(102, "CN", 8000, 0, 1);
  const cricket::AudioCodec kComfortNoiseCodec16k(103, "CN", 16000, 0, 1);

  auto codecs = media_engine->audio_send_codecs();
  codecs.push_back(kComfortNoiseCodec8k);
  codecs.push_back(kComfortNoiseCodec16k);
  media_engine->SetAudioCodecs(codecs);
}

bool HasAnyComfortNoiseCodecs(const cricket::SessionDescription* desc) {
  const auto* audio_desc = cricket::GetFirstAudioContentDescription(desc);
  for (const auto& codec : audio_desc->codecs()) {
    if (codec.name == "CN") {
      return true;
    }
  }
  return false;
}

TEST_F(PeerConnectionMediaUnitTest,
       CreateOfferWithNoVoiceActivityDetectionIncludesNoComfortNoiseCodecs) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  AddComfortNoiseCodecsToSend(caller->media_engine());

  RTCOfferAnswerOptions options;
  options.voice_activity_detection = false;
  auto offer = caller->CreateOffer(options);

  EXPECT_FALSE(HasAnyComfortNoiseCodecs(offer->description()));
}

TEST_F(PeerConnectionMediaUnitTest,
       CreateAnswerWithNoVoiceActivityDetectionIncludesNoComfortNoiseCodecs) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  AddComfortNoiseCodecsToSend(caller->media_engine());
  auto callee = CreatePeerConnectionWithAudioVideo();
  AddComfortNoiseCodecsToSend(callee->media_engine());

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  RTCOfferAnswerOptions options;
  options.voice_activity_detection = false;
  auto answer = callee->CreateAnswer(options);

  EXPECT_FALSE(HasAnyComfortNoiseCodecs(answer->description()));
}

// The following test group verifies that we reject answers with invalid media
// sections as per RFC 3264.

class PeerConnectionMediaInvalidMediaUnitTest
    : public PeerConnectionMediaUnitTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string,
                     std::function<void(cricket::SessionDescription*)>,
                     std::string>> {
 protected:
  PeerConnectionMediaInvalidMediaUnitTest() {
    mutator_ = std::get<1>(GetParam());
    expected_error_ = std::get<2>(GetParam());
  }

  std::function<void(cricket::SessionDescription*)> mutator_;
  std::string expected_error_;
};

TEST_P(PeerConnectionMediaInvalidMediaUnitTest, FailToSetRemoteAnswer) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  mutator_(answer->description());

  std::string error;
  ASSERT_FALSE(caller->SetRemoteDescription(std::move(answer), &error));
  EXPECT_EQ("Failed to set remote answer sdp: " + expected_error_, error);
}

TEST_P(PeerConnectionMediaInvalidMediaUnitTest, FailToSetLocalAnswer) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  mutator_(answer->description());

  std::string error;
  ASSERT_FALSE(callee->SetLocalDescription(std::move(answer), &error));
  EXPECT_EQ("Failed to set local answer sdp: " + expected_error_, error);
}

void RemoveVideoContent(cricket::SessionDescription* desc) {
  auto content_name = cricket::GetFirstVideoContent(desc)->name;
  desc->RemoveContentByName(content_name);
  desc->RemoveTransportInfoByName(content_name);
}

void RenameVideoContent(cricket::SessionDescription* desc) {
  auto* video_content = cricket::GetFirstVideoContent(desc);
  auto* transport_info = desc->GetTransportInfoByName(video_content->name);
  video_content->name = "video_renamed";
  transport_info->content_name = video_content->name;
}

void ReverseMediaContent(cricket::SessionDescription* desc) {
  std::reverse(desc->contents().begin(), desc->contents().end());
  std::reverse(desc->transport_infos().begin(), desc->transport_infos().end());
}

constexpr char kMLinesOutOfOrder[] =
    "The order of m-lines in answer doesn't match order in offer. Rejecting "
    "answer.";

INSTANTIATE_TEST_CASE_P(
    PeerConnectionMediaUnitTest,
    PeerConnectionMediaInvalidMediaUnitTest,
    Values(
        std::make_tuple("remove video", RemoveVideoContent, kMLinesOutOfOrder),
        std::make_tuple("rename video", RenameVideoContent, kMLinesOutOfOrder),
        std::make_tuple("reverse media sections",
                        ReverseMediaContent,
                        kMLinesOutOfOrder)));

TEST_F(PeerConnectionMediaUnitTest, TestAVOfferWithAudioOnlyAnswer) {
  RTCOfferAnswerOptions options_reject_video;
  options_reject_video.offer_to_receive_audio =
      RTCOfferAnswerOptions::kOfferToReceiveMediaTrue;
  options_reject_video.offer_to_receive_video = 0;

  auto caller = CreatePeerConnection();
  caller->AddAudioVideoStream("s", "a", "v");
  auto callee = CreatePeerConnection();

  // Caller initially offers to send/recv audio and video.
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  // Callee accepts the audio as recv only but rejects the video.
  ASSERT_TRUE(caller->SetRemoteDescription(
      callee->CreateAnswerAndSetAsLocal(options_reject_video)));

  auto caller_voice = caller->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(caller_voice);
  EXPECT_EQ(0u, caller_voice->recv_streams().size());
  EXPECT_EQ(1u, caller_voice->send_streams().size());
  auto caller_video = caller->media_engine()->GetVideoChannel(0);
  EXPECT_FALSE(caller_video);

  // Callee adds its own audio/video stream and offers to receive audio/video
  // too.
  callee->AddAudioStream("as", "a");
  auto callee_video_stream = callee->AddVideoStream("vs", "v");
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(callee_voice);
  EXPECT_EQ(1u, callee_voice->recv_streams().size());
  EXPECT_EQ(1u, callee_voice->send_streams().size());
  auto callee_video = callee->media_engine()->GetVideoChannel(0);
  ASSERT_TRUE(callee_video);
  EXPECT_EQ(1u, callee_video->recv_streams().size());
  EXPECT_EQ(1u, callee_video->send_streams().size());

  // Callee removes video but keeps audio and rejects the video once again.
  callee->pc()->RemoveStream(callee_video_stream);
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      callee->SetLocalDescription(callee->CreateAnswer(options_reject_video)));

  callee_voice = callee->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(callee_voice);
  EXPECT_EQ(1u, callee_voice->recv_streams().size());
  EXPECT_EQ(1u, callee_voice->send_streams().size());
  callee_video = callee->media_engine()->GetVideoChannel(0);
  EXPECT_FALSE(callee_video);
}

TEST_F(PeerConnectionMediaUnitTest, TestAVOfferWithVideoOnlyAnswer) {
  // Disable the bundling here. If the media is bundled on audio
  // transport, then we can't reject the audio because switching the bundled
  // transport is not currently supported.
  // (https://bugs.chromium.org/p/webrtc/issues/detail?id=6704)
  RTCOfferAnswerOptions options_no_bundle;
  options_no_bundle.use_rtp_mux = false;
  RTCOfferAnswerOptions options_reject_audio = options_no_bundle;
  options_reject_audio.offer_to_receive_audio = 0;
  options_reject_audio.offer_to_receive_video =
      RTCOfferAnswerOptions::kMaxOfferToReceiveMedia;

  auto caller = CreatePeerConnection();
  caller->AddAudioVideoStream("s", "a", "v");
  auto callee = CreatePeerConnection();

  // Caller initially offers to send/recv audio and video.
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  // Callee accepts the video as recv only but rejects the audio.
  ASSERT_TRUE(caller->SetRemoteDescription(
      callee->CreateAnswerAndSetAsLocal(options_reject_audio)));

  auto caller_voice = caller->media_engine()->GetVoiceChannel(0);
  EXPECT_FALSE(caller_voice);
  auto caller_video = caller->media_engine()->GetVideoChannel(0);
  ASSERT_TRUE(caller_video);
  EXPECT_EQ(0u, caller_video->recv_streams().size());
  EXPECT_EQ(1u, caller_video->send_streams().size());

  // Callee adds its own audio/video stream and offers to receive audio/video
  // too.
  auto callee_audio_stream = callee->AddAudioStream("as", "a");
  callee->AddVideoStream("vs", "v");
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(caller->SetRemoteDescription(
      callee->CreateAnswerAndSetAsLocal(options_no_bundle)));

  auto callee_voice = callee->media_engine()->GetVoiceChannel(0);
  ASSERT_TRUE(callee_voice);
  EXPECT_EQ(1u, callee_voice->recv_streams().size());
  EXPECT_EQ(1u, callee_voice->send_streams().size());
  auto callee_video = callee->media_engine()->GetVideoChannel(0);
  ASSERT_TRUE(callee_video);
  EXPECT_EQ(1u, callee_video->recv_streams().size());
  EXPECT_EQ(1u, callee_video->send_streams().size());

  // Callee removes audio but keeps video and rejects the audio once again.
  callee->pc()->RemoveStream(callee_audio_stream);
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      callee->SetLocalDescription(callee->CreateAnswer(options_reject_audio)));

  callee_voice = callee->media_engine()->GetVoiceChannel(0);
  EXPECT_FALSE(callee_voice);
  callee_video = callee->media_engine()->GetVideoChannel(0);
  ASSERT_TRUE(callee_video);
  EXPECT_EQ(1u, callee_video->recv_streams().size());
  EXPECT_EQ(1u, callee_video->send_streams().size());
}

// Tests that if the underlying video encoder fails to be initialized (signaled
// by failing to set send codecs), the PeerConnection signals the error to the
// client.
TEST_F(PeerConnectionMediaUnitTest, VideoEncoderErrorPropagatedToClients) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto video_channel = caller->media_engine()->GetVideoChannel(0);
  video_channel->set_fail_set_send_codecs(true);

  EXPECT_FALSE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));
}

// Tests that if the underlying video encoder fails once then subsequent
// attempts at setting the local/remote description will also fail, even if
// SetSendCodecs no longer fails.
TEST_F(PeerConnectionMediaUnitTest,
       FailToApplyDescriptionIfVideoEncoderHasEverFailed) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto video_channel = caller->media_engine()->GetVideoChannel(0);
  video_channel->set_fail_set_send_codecs(true);

  EXPECT_FALSE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  video_channel->set_fail_set_send_codecs(false);

  EXPECT_FALSE(caller->SetRemoteDescription(callee->CreateAnswer()));
  EXPECT_FALSE(caller->SetLocalDescription(caller->CreateOffer()));
}

void RenameContent(cricket::SessionDescription* desc,
                   const std::string& old_name,
                   const std::string& new_name) {
  auto* content = desc->GetContentByName(old_name);
  RTC_DCHECK(content);
  content->name = new_name;
  auto* transport = desc->GetTransportInfoByName(old_name);
  RTC_DCHECK(transport);
  transport->content_name = new_name;
}

// Tests that an answer responds with the same MIDs as the offer.
TEST_F(PeerConnectionMediaUnitTest, AnswerHasSameMidsAsOffer) {
  const std::string kAudioMid = "not default1";
  const std::string kVideoMid = "not default2";

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  auto offer = caller->CreateOffer();
  RenameContent(offer->description(), cricket::CN_AUDIO, kAudioMid);
  RenameContent(offer->description(), cricket::CN_VIDEO, kVideoMid);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  EXPECT_EQ(kAudioMid,
            cricket::GetFirstAudioContent(answer->description())->name);
  EXPECT_EQ(kVideoMid,
            cricket::GetFirstVideoContent(answer->description())->name);
}

// Test that if the callee creates a re-offer, the MIDs are the same as the
// original offer.
TEST_F(PeerConnectionMediaUnitTest, ReOfferHasSameMidsAsFirstOffer) {
  const std::string kAudioMid = "not default1";
  const std::string kVideoMid = "not default2";

  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  auto offer = caller->CreateOffer();
  RenameContent(offer->description(), cricket::CN_AUDIO, kAudioMid);
  RenameContent(offer->description(), cricket::CN_VIDEO, kVideoMid);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));

  auto reoffer = callee->CreateOffer();
  EXPECT_EQ(kAudioMid,
            cricket::GetFirstAudioContent(reoffer->description())->name);
  EXPECT_EQ(kVideoMid,
            cricket::GetFirstVideoContent(reoffer->description())->name);
}

}  // namespace
