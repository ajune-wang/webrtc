/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
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
namespace {
using Capture = VideoStreamConfig::Source::Capture;
using ContentType = VideoStreamConfig::Encoder::ContentType;
using Codec = VideoStreamConfig::Encoder::Codec;
using CodecImpl = VideoStreamConfig::Encoder::Implementation;

}  // namespace
TEST(VideoStreamTest, ReceivesFramesFromMultipleVideoStreams) {
  TimeDelta kRunTime = TimeDelta::ms(1000);
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
      c->source.capture = Capture::kVideoFile;
      c->source.video_file.name = "foreman_cif";
      c->source.video_file.width = 352;
      c->source.video_file.height = 288;
      c->encoder.content_type = ContentType::kScreen;
      c->source.framerate = kFrameRates[0];
      c->encoder.implementation = CodecImpl::kSoftware;
      c->encoder.codec = Codec::kVideoCodecVP8;
    });
    s.CreateVideoStream(route->forward(), [&](VideoStreamConfig* c) {
      c->analyzer.frame_quality_handler = [&](const VideoFrameQualityInfo&) {
        frame_counts[1]++;
      };
      c->source.capture = Capture::kGenerator;
      c->source.generator.width = 640;
      c->source.generator.height = 480;
      c->source.framerate = kFrameRates[1];
      c->encoder.implementation = CodecImpl::kSoftware;
      c->encoder.codec = Codec::kVideoCodecVP9;
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

TEST(VideoStreamTest, RecievesVp8SimulcastFrames) {
  TimeDelta kRunTime = TimeDelta::ms(100);
  int kFrameRate = 15;

  std::atomic<int> frame_count(0);
  {
    Scenario s;
    auto route = s.CreateRoutes(s.CreateClient("caller", CallClientConfig()),
                                {s.CreateSimulationNode(NetworkNodeConfig())},
                                s.CreateClient("callee", CallClientConfig()),
                                {s.CreateSimulationNode(NetworkNodeConfig())});
    s.CreateVideoStream(route->forward(), [&](VideoStreamConfig* c) {
      c->analyzer.frame_quality_handler = [&](const VideoFrameQualityInfo&) {
        frame_count++;
      };
      c->source.capture = Capture::kGenerator;
      c->source.framerate = kFrameRate;
      c->encoder.implementation = CodecImpl::kSoftware;
      c->encoder.codec = Codec::kVideoCodecVP8;
    });
  }
  const int kExpectedCount =
      static_cast<int>(kRunTime.seconds<double>() * kFrameRate);

  EXPECT_GE(frame_count, kExpectedCount - 1);
}
}  // namespace test
}  // namespace webrtc
