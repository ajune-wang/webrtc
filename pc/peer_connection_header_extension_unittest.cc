/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <tuple>

#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "media/base/fake_media_engine.h"
#include "p2p/base/fake_port_allocator.h"
#include "pc/media_session.h"
#include "pc/peer_connection_wrapper.h"
#include "rtc_base/gunit.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gmock.h"

namespace webrtc {

using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Return;
using ::testing::Values;

class PeerConnectionHeaderExtensionTest
    : public ::testing::TestWithParam<
          std::tuple<cricket::MediaType, SdpSemantics>> {
 protected:
  PeerConnectionHeaderExtensionTest()
      : virtual_socket_server_(new rtc::VirtualSocketServer()),
        main_(virtual_socket_server_.get()) {}

  std::tuple<std::unique_ptr<PeerConnectionWrapper>,
             cricket::FakeVoiceEngine*,
             cricket::FakeVideoEngine*>
  CreatePeerConnection(SdpSemantics semantics) {
    auto voice = std::make_unique<cricket::FakeVoiceEngine>();
    auto video = std::make_unique<cricket::FakeVideoEngine>();
    cricket::FakeVoiceEngine* voice_ptr = voice.get();
    cricket::FakeVideoEngine* video_ptr = video.get();
    auto media_engine = std::make_unique<cricket::CompositeMediaEngine>(
        std::move(voice), std::move(video));
    PeerConnectionFactoryDependencies factory_dependencies;
    factory_dependencies.network_thread = rtc::Thread::Current();
    factory_dependencies.worker_thread = rtc::Thread::Current();
    factory_dependencies.signaling_thread = rtc::Thread::Current();
    factory_dependencies.task_queue_factory = CreateDefaultTaskQueueFactory();
    factory_dependencies.media_engine = std::move(media_engine);
    factory_dependencies.call_factory = CreateCallFactory();
    factory_dependencies.event_log_factory =
        std::make_unique<RtcEventLogFactory>(
            factory_dependencies.task_queue_factory.get());

    auto pc_factory =
        CreateModularPeerConnectionFactory(std::move(factory_dependencies));

    auto fake_port_allocator = std::make_unique<cricket::FakePortAllocator>(
        rtc::Thread::Current(), nullptr);
    auto observer = std::make_unique<MockPeerConnectionObserver>();
    PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = semantics;
    auto pc = pc_factory->CreatePeerConnection(
        config, std::move(fake_port_allocator), nullptr, observer.get());
    observer->SetPeerConnectionInterface(pc.get());
    return std::make_tuple(std::make_unique<PeerConnectionWrapper>(
                               pc_factory, pc, std::move(observer)),
                           voice_ptr, video_ptr);
  }

  std::unique_ptr<rtc::VirtualSocketServer> virtual_socket_server_;
  rtc::AutoSocketServerThread main_;
};

TEST_P(PeerConnectionHeaderExtensionTest, TransceiverOffersHeaderExtensions) {
  cricket::MediaType media_type;
  SdpSemantics semantics;
  std::tie(media_type, semantics) = GetParam();
  std::unique_ptr<PeerConnectionWrapper> wrapper;
  cricket::FakeVoiceEngine* voice;
  cricket::FakeVideoEngine* video;
  std::tie(wrapper, voice, video) = CreatePeerConnection(semantics);
  std::vector<RtpHeaderExtensionCapability> extensions = {
      RtpHeaderExtensionCapability("uri1", 1,
                                   RtpTransceiverDirection::kStopped),
      RtpHeaderExtensionCapability("uri2", 2,
                                   RtpTransceiverDirection::kSendOnly),
      RtpHeaderExtensionCapability("uri3", 3,
                                   RtpTransceiverDirection::kRecvOnly),
      RtpHeaderExtensionCapability("uri4", 4,
                                   RtpTransceiverDirection::kSendRecv)};
  if (media_type == cricket::MediaType::MEDIA_TYPE_AUDIO)
    voice->SetRtpHeaderExtensions(extensions);
  else
    video->SetRtpHeaderExtensions(extensions);
  if (semantics == SdpSemantics::kUnifiedPlan) {
    auto transceiver = wrapper->AddTransceiver(media_type);
    EXPECT_EQ(transceiver->header_extensions_to_offer(), extensions);
  }
  EXPECT_THAT(wrapper->pc_factory()
                  ->GetRtpSenderCapabilities(media_type)
                  .header_extensions,
              ElementsAre(Field(&RtpHeaderExtensionCapability::uri, "uri2"),
                          Field(&RtpHeaderExtensionCapability::uri, "uri3"),
                          Field(&RtpHeaderExtensionCapability::uri, "uri4")));
  EXPECT_EQ(wrapper->pc_factory()
                ->GetRtpReceiverCapabilities(media_type)
                .header_extensions,
            wrapper->pc_factory()
                ->GetRtpSenderCapabilities(media_type)
                .header_extensions);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PeerConnectionHeaderExtensionTest,
    Combine(Values(SdpSemantics::kPlanB, SdpSemantics::kUnifiedPlan),
            Values(cricket::MediaType::MEDIA_TYPE_AUDIO,
                   cricket::MediaType::MEDIA_TYPE_VIDEO)),
    [](const testing::TestParamInfo<
        PeerConnectionHeaderExtensionTest::ParamType>& info) {
      cricket::MediaType media_type;
      SdpSemantics semantics;
      std::tie(media_type, semantics) = info.param;
      return (rtc::StringBuilder("With")
              << (semantics == SdpSemantics::kPlanB ? "PlanB" : "UnifiedPlan")
              << "And"
              << (media_type == cricket::MediaType::MEDIA_TYPE_AUDIO ? "Voice"
                                                                     : "Video")
              << "Engine")
          .str();
    });

}  // namespace webrtc
