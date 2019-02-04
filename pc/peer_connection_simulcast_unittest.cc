/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/rtp_transceiver_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;

using cricket::MediaContentDescription;
using cricket::RidDescription;
using cricket::SimulcastDescription;
using cricket::SimulcastLayer;

namespace webrtc {

class PeerConnectionSimulcastTests : public testing::Test {
 public:
  PeerConnectionSimulcastTests()
      : pc_factory_(
            CreatePeerConnectionFactory(rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        FakeAudioCaptureModule::Create(),
                                        CreateBuiltinAudioEncoderFactory(),
                                        CreateBuiltinAudioDecoderFactory(),
                                        CreateBuiltinVideoEncoderFactory(),
                                        CreateBuiltinVideoDecoderFactory(),
                                        nullptr,
                                        nullptr)) {}

  rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      MockPeerConnectionObserver* observer) {
    PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    PeerConnectionDependencies pcd(observer);
    auto pc = pc_factory_->CreatePeerConnection(config, std::move(pcd));
    EXPECT_TRUE(pc.get());
    observer->SetPeerConnectionInterface(pc);
    return pc;
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnectionWrapper() {
    auto observer = absl::make_unique<MockPeerConnectionObserver>();
    auto pc = CreatePeerConnection(observer.get());
    return absl::make_unique<PeerConnectionWrapper>(pc_factory_, pc,
                                                    std::move(observer));
  }

  RtpTransceiverInit CreateTransceiverInit(const std::vector<std::string>& rids,
                                           const std::vector<bool>& active) {
    RTC_DCHECK_EQ(rids.size(), active.size());
    RtpTransceiverInit init;
    for (size_t i = 0; i < rids.size(); i++) {
      RtpEncodingParameters encoding;
      encoding.rid = rids[i];
      encoding.active = active[i];
      init.send_encodings.push_back(encoding);
    }
    return init;
  }

  rtc::scoped_refptr<RtpTransceiverInterface> AddTransceiver(
      PeerConnectionWrapper* pc,
      const std::vector<std::string>& rids,
      const std::vector<bool>& active) {
    auto init = CreateTransceiverInit(rids, active);
    return pc->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  }

  SimulcastDescription RemoveSimulcast(SessionDescriptionInterface* sd) {
    auto mcd = sd->description()->contents()[0].media_description();
    auto result = mcd->simulcast_description();
    mcd->set_simulcast_description(SimulcastDescription());
    return result;
  }

  void AddRequestToReceiveSimulcast(const std::vector<std::string>& rids,
                                    const std::vector<bool>& active,
                                    SessionDescriptionInterface* sd) {
    RTC_DCHECK_EQ(rids.size(), active.size());
    auto mcd = sd->description()->contents()[0].media_description();
    SimulcastDescription simulcast;
    auto& receive_layers = simulcast.receive_layers();
    for (size_t i = 0; i < rids.size(); i++) {
      // Flip boolean because |active| != |is_paused|.
      receive_layers.AddLayer(SimulcastLayer(rids[i], !active[i]));
    }
    mcd->set_simulcast_description(simulcast);
  }

  void ValidateTransceiverParameters(
      rtc::scoped_refptr<RtpTransceiverInterface> transceiver,
      const std::vector<std::string>& rids,
      const std::vector<bool> active) {
    auto parameters = transceiver->sender()->GetParameters();
    std::vector<std::string> result_rids;
    absl::c_transform(
        parameters.encodings, std::back_inserter(result_rids),
        [](const RtpEncodingParameters& encoding) { return encoding.rid; });
    EXPECT_THAT(result_rids, ElementsAreArray(rids));

    std::vector<bool> result_active;
    absl::c_transform(
        parameters.encodings, std::back_inserter(result_active),
        [](const RtpEncodingParameters& encoding) { return encoding.active; });
    EXPECT_THAT(result_active, ElementsAreArray(active));
  }

 private:
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
  MockPeerConnectionObserver observer_;
};

// Validates that RIDs are supported arguments when adding a transceiver.
// This test is currently disabled because a single RID is not supported.
TEST_F(PeerConnectionSimulcastTests, DISABLED_CanCreateTransceiverWithRid) {
  auto pc = CreatePeerConnectionWrapper();
  auto transceiver = AddTransceiver(pc.get(), {"f"}, {true});
  EXPECT_TRUE(transceiver.get());
}

TEST_F(PeerConnectionSimulcastTests, SingleRidIsNotSupported) {
  auto pc_wrapper = CreatePeerConnectionWrapper();
  auto pc = pc_wrapper->pc();
  auto init = CreateTransceiverInit({"f"}, {true});
  auto error = pc->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  EXPECT_FALSE(error.ok());
}

// Validates that an error is returned if RIDs are not supplied for Simulcast.
TEST_F(PeerConnectionSimulcastTests, MustSupplyRidsInSimulcast) {
  auto pc_wrapper = CreatePeerConnectionWrapper();
  auto pc = pc_wrapper->pc();
  std::vector<std::string> rids({"f", "h", ""});
  std::vector<bool> active(rids.size(), true);
  auto init = CreateTransceiverInit(rids, active);
  auto error = pc->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  EXPECT_FALSE(error.ok());
}

// Validates that a single RID does not get negotiated.
// This test is currently disabled because a single RID is not supported.
TEST_F(PeerConnectionSimulcastTests,
       DISABLED_SingleRidIsRemovedFromSessionDescription) {
  auto pc = CreatePeerConnectionWrapper();
  auto transceiver = AddTransceiver(pc.get(), {"1"}, {true});
  auto offer = pc->CreateOfferAndSetAsLocal();
  EXPECT_TRUE(offer.get());
  auto description = offer->description();
  ASSERT_TRUE(description);
  auto contents = description->contents();
  ASSERT_EQ(1u, contents.size());
  auto content = contents[0];
  auto mcd = content.media_description();
  ASSERT_TRUE(mcd);
  auto streams = mcd->streams();
  ASSERT_EQ(1u, streams.size());
  auto stream = streams[0];
  EXPECT_FALSE(stream.has_ssrcs());
  EXPECT_FALSE(stream.has_rids());
}

// Checks that an offfer to send simulcast contains a SimulcastDescription.
TEST_F(PeerConnectionSimulcastTests, SimulcastAppearsInSessionDescription) {
  auto pc = CreatePeerConnectionWrapper();
  std::vector<std::string> rids({"f", "h", "q"});
  std::vector<bool> active(rids.size(), true);
  auto transceiver = AddTransceiver(pc.get(), rids, active);
  auto offer = pc->CreateOffer();
  EXPECT_TRUE(offer.get());
  auto description = offer->description();
  ASSERT_TRUE(description);
  auto contents = description->contents();
  ASSERT_EQ(1u, contents.size());
  auto content = contents[0];
  auto mcd = content.media_description();
  ASSERT_TRUE(mcd);
  EXPECT_TRUE(mcd->HasSimulcast());
  auto simulcast = mcd->simulcast_description();
  EXPECT_THAT(simulcast.receive_layers(), IsEmpty());
  // The size is validated separately because GetAllLayers() flattens the list.
  EXPECT_THAT(simulcast.send_layers(), SizeIs(3));
  std::vector<std::string> result_rids;
  absl::c_transform(simulcast.send_layers().GetAllLayers(),
                    std::back_inserter(result_rids),
                    [](const SimulcastLayer& layer) { return layer.rid; });
  EXPECT_THAT(result_rids, ElementsAreArray(rids));
  EXPECT_THAT(simulcast.send_layers().GetAllLayers(),
              Each(Field(&SimulcastLayer::is_paused, Eq(false))));
  auto streams = mcd->streams();
  ASSERT_EQ(1u, streams.size());
  auto stream = streams[0];
  EXPECT_FALSE(stream.has_ssrcs());
  EXPECT_TRUE(stream.has_rids());
  result_rids.clear();
  absl::c_transform(stream.rids(), std::back_inserter(result_rids),
                    [](const RidDescription& rid) { return rid.rid; });
  EXPECT_THAT(result_rids, ElementsAreArray(rids));
}

// Checks that Simulcast layers propagate to the sender parameters.
TEST_F(PeerConnectionSimulcastTests, SimulcastLayersAreSetInSender) {
  auto pc1 = CreatePeerConnectionWrapper();
  auto pc2 = CreatePeerConnectionWrapper();
  std::vector<std::string> rids({"f", "h", "q"});
  std::vector<bool> active(rids.size(), true);
  auto transceiver = AddTransceiver(pc1.get(), rids, active);
  auto offer = pc1->CreateOfferAndSetAsLocal();
  ValidateTransceiverParameters(transceiver, rids, active);

  // Remove simulcast as the second peer connection won't support it.
  auto simulcast = RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(pc2->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = pc2->CreateAnswerAndSetAsLocal();

  // Setup an answer that mimics a server accepting simulcast.
  auto mcd_answer = answer->description()->contents()[0].media_description();
  mcd_answer->mutable_streams().clear();
  auto simulcast_layers = simulcast.send_layers().GetAllLayers();
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const auto& layer : simulcast_layers) {
    receive_layers.AddLayer(layer);
  }
  EXPECT_TRUE(pc1->SetRemoteDescription(std::move(answer), &error)) << error;
  ValidateTransceiverParameters(transceiver, rids, active);
}

// Checks that paused Simulcast layers propagate to the sender parameters.
TEST_F(PeerConnectionSimulcastTests, PausedSimulcastLayersAreDisabledInSender) {
  auto pc1 = CreatePeerConnectionWrapper();
  auto pc2 = CreatePeerConnectionWrapper();
  std::vector<std::string> rids({"f", "h", "q"});
  std::vector<bool> active({true, false, true});
  std::vector<bool> server_active({true, false, false});
  auto transceiver = AddTransceiver(pc1.get(), rids, active);
  auto offer = pc1->CreateOfferAndSetAsLocal();
  ValidateTransceiverParameters(transceiver, rids, active);

  // Remove simulcast as the second peer connection won't support it.
  RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(pc2->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = pc2->CreateAnswerAndSetAsLocal();

  // Setup an answer that mimics a server accepting simulcast.
  auto mcd_answer = answer->description()->contents()[0].media_description();
  mcd_answer->mutable_streams().clear();
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  ASSERT_EQ(rids.size(), server_active.size());
  for (size_t i = 0; i < rids.size(); i++) {
    // Using |!server_active| because |paused != active|.
    receive_layers.AddLayer(SimulcastLayer(rids[i], !server_active[i]));
  }
  EXPECT_TRUE(pc1->SetRemoteDescription(std::move(answer), &error)) << error;
  ValidateTransceiverParameters(transceiver, rids, server_active);
}

// Checks that when Simulcast is not supported by the remote party, then all
// the layers (except the first) are marked as disabled.
TEST_F(PeerConnectionSimulcastTests, SimulcastRejectedDisablesExtraLayers) {
  auto pc1 = CreatePeerConnectionWrapper();
  auto pc2 = CreatePeerConnectionWrapper();
  std::vector<std::string> rids({"1", "2", "3", "4"});
  std::vector<bool> active(rids.size(), true);
  auto transceiver = AddTransceiver(pc1.get(), rids, active);
  auto offer = pc1->CreateOfferAndSetAsLocal();
  // Remove simulcast as the second peer connection won't support it.
  RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(pc2->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = pc2->CreateAnswerAndSetAsLocal();
  EXPECT_TRUE(pc1->SetRemoteDescription(std::move(answer), &error)) << error;
  std::vector<bool> expected_active(rids.size(), false);
  expected_active[0] = true;
  // The number of layers does not change.
  ValidateTransceiverParameters(transceiver, rids, expected_active);
}

// Checks that if Simulcast is supported by remote party, but some layer is
// rejected, then only that layer is marked as disabled.
TEST_F(PeerConnectionSimulcastTests, RejectedSimulcastLayersAreDeactivated) {
  auto pc1 = CreatePeerConnectionWrapper();
  auto pc2 = CreatePeerConnectionWrapper();
  std::vector<std::string> rids({"1", "2", "3", "4"});
  std::vector<bool> active(rids.size(), true);
  auto transceiver = AddTransceiver(pc1.get(), rids, active);
  auto offer = pc1->CreateOfferAndSetAsLocal();
  ValidateTransceiverParameters(transceiver, rids, active);
  // Remove simulcast as the second peer connection won't support it.
  auto removed_simulcast = RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(pc2->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = pc2->CreateAnswerAndSetAsLocal();
  auto mcd_answer = answer->description()->contents()[0].media_description();
  // Setup the answer to look like a server response.
  // Remove one of the layers to reject it in the answer.
  auto simulcast_layers = removed_simulcast.send_layers().GetAllLayers();
  simulcast_layers.erase(simulcast_layers.begin());
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const auto& layer : simulcast_layers) {
    receive_layers.AddLayer(layer);
  }
  ASSERT_TRUE(mcd_answer->HasSimulcast());
  EXPECT_TRUE(pc1->SetRemoteDescription(std::move(answer), &error)) << error;
  active[0] = false;
  ValidateTransceiverParameters(transceiver, rids, active);
}

// Checks that simulcast is set up correctly when the server sends an offer
// requesting to receive simulcast.
TEST_F(PeerConnectionSimulcastTests, ServerSendsOfferToReceiveSimulcast) {
  auto pc1 = CreatePeerConnectionWrapper();
  auto pc2 = CreatePeerConnectionWrapper();
  std::vector<std::string> rids({"f", "h", "q"});
  std::vector<bool> active(rids.size(), true);
  AddTransceiver(pc1.get(), rids, active);
  auto offer = pc1->CreateOfferAndSetAsLocal();
  // Remove simulcast as a sender and set it up as a receiver.
  RemoveSimulcast(offer.get());
  AddRequestToReceiveSimulcast(rids, active, offer.get());
  std::string error;
  EXPECT_TRUE(pc2->SetRemoteDescription(std::move(offer), &error)) << error;
  auto transceiver = pc2->pc()->GetTransceivers()[0];
  transceiver->SetDirection(RtpTransceiverDirection::kSendRecv);
  pc2->CreateAnswerAndSetAsLocal();
  ValidateTransceiverParameters(transceiver, rids, active);
}

// Checks that SetRemoteDescription doesn't attempt to recycle a transceiver
// when simulcast is requested by the server.
TEST_F(PeerConnectionSimulcastTests, TransceiverIsNotRecycledWithSimulcast) {
  auto pc1 = CreatePeerConnectionWrapper();
  auto pc2 = CreatePeerConnectionWrapper();
  std::vector<std::string> rids({"f", "h", "q"});
  std::vector<bool> active(rids.size(), true);
  AddTransceiver(pc1.get(), rids, active);
  auto offer = pc1->CreateOfferAndSetAsLocal();
  // Remove simulcast as a sender and set it up as a receiver.
  RemoveSimulcast(offer.get());
  AddRequestToReceiveSimulcast(rids, active, offer.get());
  // Call AddTrack so that a transceiver is created.
  pc2->AddVideoTrack("fake_track");
  std::string error;
  EXPECT_TRUE(pc2->SetRemoteDescription(std::move(offer), &error)) << error;
  auto transceiver = pc2->pc()->GetTransceivers()[1];
  transceiver->SetDirection(RtpTransceiverDirection::kSendRecv);
  pc2->CreateAnswerAndSetAsLocal();
  ValidateTransceiverParameters(transceiver, rids, active);
}

}  // namespace webrtc
