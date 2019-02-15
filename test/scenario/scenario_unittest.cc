/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <atomic>

#include "test/gtest.h"
#include "test/scenario/scenario.h"
namespace webrtc {
namespace test {
TEST(ScenarioTest, StartsAndStopsWithoutErrors) {
  Scenario s;
  CallClientConfig call_client_config;
  call_client_config.transport.rates.start_rate = DataRate::kbps(300);
  auto* alice = s.CreateClient("alice", call_client_config);
  auto* bob = s.CreateClient("bob", call_client_config);
  NetworkNodeConfig network_config;
  auto alice_net = s.CreateSimulationNode(network_config);
  auto bob_net = s.CreateSimulationNode(network_config);
  auto route = s.CreateRoutes(alice, {alice_net}, bob, {bob_net});

  VideoStreamConfig video_stream_config;
  s.CreateVideoStream(route->forward(), video_stream_config);
  s.CreateVideoStream(route->reverse(), video_stream_config);

  AudioStreamConfig audio_stream_config;
  audio_stream_config.encoder.min_rate = DataRate::kbps(6);
  audio_stream_config.encoder.max_rate = DataRate::kbps(64);
  audio_stream_config.encoder.allocate_bitrate = true;
  audio_stream_config.stream.in_bandwidth_estimation = false;
  s.CreateAudioStream(route->forward(), audio_stream_config);
  s.CreateAudioStream(route->reverse(), audio_stream_config);

  CrossTrafficConfig cross_traffic_config;
  s.CreateCrossTraffic({alice_net}, cross_traffic_config);

  bool packet_received = false;
  s.NetworkDelayedAction({alice_net, bob_net}, 100,
                         [&packet_received] { packet_received = true; });
  bool bitrate_changed = false;
  s.Every(TimeDelta::ms(10), [alice, bob, &bitrate_changed] {
    if (alice->GetStats().send_bandwidth_bps != 300000 &&
        bob->GetStats().send_bandwidth_bps != 300000)
      bitrate_changed = true;
  });
  s.RunUntil(TimeDelta::seconds(2), TimeDelta::ms(5),
             [&bitrate_changed, &packet_received] {
               return packet_received && bitrate_changed;
             });
  EXPECT_TRUE(packet_received);
  EXPECT_TRUE(bitrate_changed);
}

TEST(ScenarioTest, ReceivesFramesFromMultipleVideoStreams) {
  using Src = VideoStreamConfig::Source;
  using Enc = VideoStreamConfig::Encoder;
  TimeDelta kRunTime = TimeDelta::ms(500);
  std::vector<int> kFrameRates = {5, 15};

  std::deque<std::atomic<int>> frame_counts(2);
  frame_counts[0] = 0;
  frame_counts[1] = 0;
  {
    Scenario s;
    auto route = s.CreateRoutes(s.CreateClient("caller", CallClientConfig()),
                                {s.CreateSimulationNode(NetworkNodeConfig())},
                                s.CreateClient("callee", CallClientConfig()),
                                {s.CreateSimulationNode(NetworkNodeConfig())});

    s.CreateVideoStream(route->forward(), [&](VideoStreamConfig* c) {
      c->analyzer.frame_quality_handler = [&](const VideoFrameQualityInfo&) {
        frame_counts[0]++;
      };
      c->source.capture = Src::Capture::kVideoFile;
      c->source.video_file.name = "foreman_320x240";
      c->source.video_file.width = 320;
      c->source.video_file.height = 240;
      c->encoder.content_type = Enc::ContentType::kScreen;
      c->source.framerate = kFrameRates[0];
      c->encoder.implementation = Enc::Implementation::kSoftware;
      c->encoder.codec = Enc::Codec::kVideoCodecVP8;
    });
    s.CreateVideoStream(route->forward(), [&](VideoStreamConfig* c) {
      c->analyzer.frame_quality_handler = [&](const VideoFrameQualityInfo&) {
        frame_counts[1]++;
      };
      c->source.capture = Src::Capture::kGenerator;
      c->source.generator.width = 640;
      c->source.generator.height = 480;
      c->source.framerate = kFrameRates[1];
      c->encoder.implementation = Enc::Implementation::kSoftware;
      c->encoder.codec = Enc::Codec::kVideoCodecVP9;
    });
    s.RunFor(kRunTime);
  }
  std::vector<int> expected_counts;
  for (auto fps : kFrameRates)
    expected_counts.push_back(
        static_cast<int>(kRunTime.seconds<double>() * fps));

  EXPECT_GE(frame_counts[0], expected_counts[0] - 1);
  EXPECT_GE(frame_counts[1], expected_counts[1] - 1);
}
}  // namespace test
}  // namespace webrtc
