/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/stats/rtc_stats_collector_callback.h"
#include "api/stats/rtcstats_objects.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/peer_scenario/peer_scenario.h"
#include "test/peer_scenario/peer_scenario_client.h"

namespace webrtc {
namespace test {

TEST(PeerScenarioQualityTest, PsnrIsCollected) {
  VideoQualityAnalyzer analyzer;
  {
    PeerScenario s(*test_info_);
    auto caller = s.CreateClient(PeerScenarioClient::Config());
    auto callee = s.CreateClient(PeerScenarioClient::Config());
    PeerScenarioClient::VideoSendTrackConfig video_conf;
    video_conf.generator.squares_video->framerate = 20;
    auto video = caller->CreateVideo("VIDEO", video_conf);
    auto link_builder = s.net()->NodeBuilder().delay_ms(100).capacity_kbps(600);
    s.AttachVideoQualityAnalyzer(&analyzer, video.track, callee);
    s.SimpleConnection(caller, callee, {link_builder.Build().node},
                       {link_builder.Build().node});
    s.ProcessMessages(TimeDelta::Seconds(2));
    // Exit scope to ensure that there's no pending tasks reporting to analyzer.
  }

  // We expect ca 40 frames to be produced, but to avoid flakiness on slow
  // machines we only test for 10.
  EXPECT_GT(analyzer.stats().render.count, 10);
  EXPECT_GT(analyzer.stats().psnr_with_freeze.Mean(), 20);
}

TEST(GoogCcPeerScenarioTest, NoBweChangeFromVideoUnmute) {
  // If transport wide sequence numbers are used for audio, and the call
  // switches from audio only to video only, there will be a sharp change in
  // packets sizes. This will create a change in propagation time which might be
  // detected as an overuse. Using separate overuse detectors for audio and
  // video avoids the issue.
  std::string audio_twcc_trials(
      "WebRTC-Audio-SendSideBwe/Enabled/"              //
      "WebRTC-SendSideBwe-WithOverhead/Enabled/"       //
      "WebRTC-Audio-LegacyOverhead/Enabled/"           //
      "WebRTC-Pacer-IgnoreTransportOverhead/Enabled/"  //
      "WebRTC-Audio-AlrProbing/Disabled/");
  std::string separate_audio_video(
      ""
      //      "WebRTC-Bwe-SeparateAudioPackets/"
      //      "enabled:true,packet_threshold:15,time_threshold:1000ms/"
  );
  const std::string combined_trials = audio_twcc_trials + separate_audio_video;
  // N.B. The combined string needs to outlive the ScopedFieldTrial!
  ScopedFieldTrials field_trial(combined_trials.c_str());
  PeerScenario s(*test_info_);
  auto* caller = s.CreateClient(PeerScenarioClient::Config());
  auto* callee = s.CreateClient(PeerScenarioClient::Config());

  BuiltInNetworkBehaviorConfig net_conf;
  net_conf.link_capacity_kbps = 350;
  net_conf.queue_delay_ms = 50;
  auto node_builder = s.net()->NodeBuilder().config(net_conf);
  auto send = node_builder.Build();
  auto ret_node = node_builder.Build().node;

  s.net()->CreateRoute(caller->endpoint(), {send.node}, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {ret_node}, caller->endpoint());

  PeerScenarioClient::VideoSendTrackConfig video_conf;
  video_conf.generator.squares_video->framerate = 15;
  auto video = caller->CreateVideo("VIDEO", video_conf);
  auto audio = caller->CreateAudio("AUDIO", cricket::AudioOptions());
  auto signaling = s.ConnectSignaling(caller, callee, {send.node}, {ret_node});
  signaling.StartIceSignaling();
  RtpHeaderExtensionMap extension_map;
  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp(
      [](SessionDescriptionInterface* /*offer*/) {
        // Optionally modify offer
      },
      [&](const SessionDescriptionInterface& /*answer*/) {
        // Optionally process answer
        offer_exchange_done = true;
      });
  RTC_CHECK(s.WaitAndProcess(&offer_exchange_done));

  // Limit the encoder bitrate to ensure that there are no actual BWE overuses.
  EXPECT_EQ(caller->pc()->GetSenders().size(), 2u);  // 2 senders.
  int num_video_streams = 0;
  for (auto& rtp_sender : caller->pc()->GetSenders()) {
    auto parameters = rtp_sender->GetParameters();
    EXPECT_EQ(parameters.encodings.size(), 1u);  // 1 stream per sender.
    for (auto& encoding_parameters : parameters.encodings) {
      if (encoding_parameters.ssrc == video.sender->ssrc()) {
        num_video_streams++;
        encoding_parameters.max_bitrate_bps = 220000;
        encoding_parameters.max_framerate = 15;
      }
    }
    rtp_sender->SetParameters(parameters);
  }
  EXPECT_EQ(num_video_streams, 1);  // Exactly 1 video stream.

  auto get_bwe = [&] {
    rtc::scoped_refptr<webrtc::MockRTCStatsCollectorCallback> callback(
        new rtc::RefCountedObject<webrtc::MockRTCStatsCollectorCallback>());
    caller->pc()->GetStats(callback);
    s.net()->time_controller()->Wait([&] { return callback->called(); });
    auto stats =
        callback->report()->GetStatsOfType<RTCIceCandidatePairStats>()[0];
    return DataRate::BitsPerSec(*stats->available_outgoing_bitrate);
  };

  s.ProcessMessages(TimeDelta::Seconds(15));
  DataRate bwe = get_bwe();
  EXPECT_GE(bwe, DataRate::KilobitsPerSec(300));

  // 10 seconds audio only. Bandwidth should not drop.
  video.capturer->Stop();
  s.ProcessMessages(TimeDelta::Seconds(10));
  EXPECT_GE(get_bwe(), bwe);

  // Resume video but stop audio. Bandwidth should not drop.
  video.capturer->Start();
  bool status = caller->pc()->RemoveTrack(audio.sender);
  EXPECT_TRUE(status);
  audio.track->set_enabled(false);
  for (int i = 0; i < 10; i++) {
    s.ProcessMessages(TimeDelta::Seconds(1));
    EXPECT_GE(get_bwe(), bwe);
  }
}

}  // namespace test
}  // namespace webrtc
