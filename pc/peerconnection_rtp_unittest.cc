/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/jsep.h"
#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "pc/mediastream.h"
#include "pc/mediastreamtrack.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/mockpeerconnectionobservers.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"

// This file contains tests for RTP Media API-related behavior of
// |webrtc::PeerConnection|, see https://w3c.github.io/webrtc-pc/#rtp-media-api.

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using ::testing::UnorderedElementsAre;

class PeerConnectionRtpTest : public testing::Test {
 public:
  PeerConnectionRtpTest()
      : pc_factory_(
            CreatePeerConnectionFactory(rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        FakeAudioCaptureModule::Create(),
                                        CreateBuiltinAudioEncoderFactory(),
                                        CreateBuiltinAudioDecoderFactory(),
                                        nullptr,
                                        nullptr)) {}

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection() {
    return CreatePeerConnection(RTCConfiguration());
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnectionWithUnifiedPlan() {
    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    return CreatePeerConnection(config);
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection(
      const RTCConfiguration& config) {
    auto observer = rtc::MakeUnique<MockPeerConnectionObserver>();
    auto pc = pc_factory_->CreatePeerConnection(config, nullptr, nullptr,
                                                observer.get());
    return rtc::MakeUnique<PeerConnectionWrapper>(pc_factory_, pc,
                                                  std::move(observer));
  }

 protected:
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
};

TEST_F(PeerConnectionRtpTest, AddTrackWithoutStreamFiresOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  EXPECT_TRUE(caller->pc()->AddTrack(audio_track.get(), {}));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  // TODO(deadbeef): When no stream is handled correctly we would expect
  // |add_track_events_[0].streams| to be empty. https://crbug.com/webrtc/7933
  ASSERT_EQ(1u, callee->observer()->add_track_events_[0].streams.size());
  EXPECT_TRUE(
      callee->observer()->add_track_events_[0].streams[0]->FindAudioTrack(
          "audio_track"));
}

TEST_F(PeerConnectionRtpTest, AddTrackWithStreamFiresOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  auto stream = MediaStream::Create("audio_stream");
  EXPECT_TRUE(caller->pc()->AddTrack(audio_track.get(), {stream.get()}));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  ASSERT_EQ(1u, callee->observer()->add_track_events_[0].streams.size());
  EXPECT_EQ("audio_stream",
            callee->observer()->add_track_events_[0].streams[0]->label());
  EXPECT_TRUE(
      callee->observer()->add_track_events_[0].streams[0]->FindAudioTrack(
          "audio_track"));
}

TEST_F(PeerConnectionRtpTest, RemoveTrackWithoutStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  auto sender = caller->pc()->AddTrack(audio_track.get(), {});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

TEST_F(PeerConnectionRtpTest, RemoveTrackWithStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  auto stream = MediaStream::Create("audio_stream");
  auto sender = caller->pc()->AddTrack(audio_track.get(), {stream.get()});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

TEST_F(PeerConnectionRtpTest, RemoveTrackWithSharedStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<AudioTrackInterface> audio_track1(
      pc_factory_->CreateAudioTrack("audio_track1", nullptr));
  rtc::scoped_refptr<AudioTrackInterface> audio_track2(
      pc_factory_->CreateAudioTrack("audio_track2", nullptr));
  auto stream = MediaStream::Create("shared_audio_stream");
  std::vector<MediaStreamInterface*> streams{stream.get()};
  auto sender1 = caller->pc()->AddTrack(audio_track1.get(), streams);
  auto sender2 = caller->pc()->AddTrack(audio_track2.get(), streams);
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(2u, callee->observer()->add_track_events_.size());

  // Remove "audio_track1".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender1));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(2u, callee->observer()->add_track_events_.size());
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpReceiverInterface>>{
          callee->observer()->add_track_events_[0].receiver},
      callee->observer()->remove_track_events_);

  // Remove "audio_track2".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender2));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(2u, callee->observer()->add_track_events_.size());
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

// RtpTransceiver Tests

// Test that a transceiver created with the audio kind has the correct initial
// properties.
TEST_F(PeerConnectionRtpTest, AddTransceiverHasCorrectInitProperties) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver =
      caller->AddTransceiver(MediaStreamTrackInterface::kAudioKind);
  EXPECT_EQ(rtc::nullopt, transceiver->mid());
  EXPECT_FALSE(transceiver->stopped());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceiver->direction());
  EXPECT_EQ(rtc::nullopt, transceiver->current_direction());
}

// Test that adding a transceiver with the audio kind creates an audio sender
// and audio receiver with the receiver having a live audio track.
TEST_F(PeerConnectionRtpTest,
       AddAudioTransceiverCreatesAudioSenderAndReceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver =
      caller->AddTransceiver(MediaStreamTrackInterface::kAudioKind);

  ASSERT_TRUE(transceiver->sender());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, transceiver->sender()->media_type());

  ASSERT_TRUE(transceiver->receiver());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, transceiver->receiver()->media_type());

  auto track = transceiver->receiver()->track();
  ASSERT_TRUE(track);
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind, track->kind());
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kLive, track->state());
}

// Test that adding a transceiver with the video kind creates an video sender
// and video receiver with the receiver having a live video track.
TEST_F(PeerConnectionRtpTest,
       AddAudioTransceiverCreatesVideoSenderAndReceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver =
      caller->AddTransceiver(MediaStreamTrackInterface::kVideoKind);

  ASSERT_TRUE(transceiver->sender());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->sender()->media_type());

  ASSERT_TRUE(transceiver->receiver());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->receiver()->media_type());

  auto track = transceiver->receiver()->track();
  ASSERT_TRUE(track);
  EXPECT_EQ(MediaStreamTrackInterface::kVideoKind, track->kind());
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kLive, track->state());
}

// Test that after a call to AddTransceiver, the transceiver shows in
// GetTransceivers(), the transceiver's sender shows in GetSenders(), and the
// transceiver's receiver shows in GetReceivers().
TEST_F(PeerConnectionRtpTest, AddTransceiverShowsInLists) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver =
      caller->AddTransceiver(MediaStreamTrackInterface::kAudioKind);
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>{transceiver},
      caller->pc()->GetTransceivers());
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpSenderInterface>>{
          transceiver->sender()},
      caller->pc()->GetSenders());
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpReceiverInterface>>{
          transceiver->receiver()},
      caller->pc()->GetReceivers());
}

// Test that the direction passed in through the AddTransceiver init parameter
// is set in the returned transceiver.
TEST_F(PeerConnectionRtpTest, AddTransceiverWithDirectionIsReflected) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  auto transceiver =
      caller->AddTransceiver(MediaStreamTrackInterface::kAudioKind, init);
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly, transceiver->direction());
}

TEST_F(PeerConnectionRtpTest, AddTransceiverWithInvalidKindReturnsError) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto result = caller->pc()->AddTransceiver("invalid kind");
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, result.error().type());
}

// Test that calling AddTransceiver with a track creates a transceiver which has
// its sender's track set to the passed-in track.
TEST_F(PeerConnectionRtpTest, AddTransceiverWithTrackCreatesSenderWithTrack) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack("audio track");
  auto transceiver = caller->AddTransceiver(audio_track);

  auto sender = transceiver->sender();
  ASSERT_TRUE(sender->track());
  EXPECT_EQ(audio_track, sender->track());

  auto receiver = transceiver->receiver();
  ASSERT_TRUE(receiver->track());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind, receiver->track()->kind());
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kLive,
            receiver->track()->state());
}

// Test that calling AddTransceiver twice with the same track creates distinct
// transceivers, senders with the same track.
TEST_F(PeerConnectionRtpTest,
       AddTransceiverTwiceWithSameTrackCreatesMultipleTransceivers) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack("audio track");

  auto transceiver1 = caller->AddTransceiver(audio_track);
  auto transceiver2 = caller->AddTransceiver(audio_track);

  EXPECT_NE(transceiver1, transceiver2);

  auto sender1 = transceiver1->sender();
  auto sender2 = transceiver2->sender();
  EXPECT_NE(sender1, sender2);
  EXPECT_EQ(audio_track, sender1->track());
  EXPECT_EQ(audio_track, sender2->track());

  EXPECT_THAT(caller->pc()->GetTransceivers(),
              UnorderedElementsAre(transceiver1, transceiver2));
  EXPECT_THAT(caller->pc()->GetSenders(),
              UnorderedElementsAre(sender1, sender2));
}

}  // namespace webrtc
