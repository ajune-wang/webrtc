/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videocodec_test_fixture_impl.h"

#include <utility>
#include <vector>

#include "api/test/videocodec_test_fixture.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {

namespace {

const size_t kNumFrames = 2;
const RateProfile kRateProfile = {.target_kbps = 300,
                                  .input_fps = 30,
                                  .frame_num = 0};

webrtc::test::VideoCodecTestFixture::Config CreateConfig() {
  VideoCodecTestFixture::Config config;
  config.filename = "foreman_cif";
  config.filepath = ResourcePath(config.filename, "yuv");
  config.num_frames = kNumFrames;
  config.SetCodecSettings("VP8", /*num_simulcast_streams=*/1,
                          /*num_spatial_layers=*/1, /*num_temporal_layers=*/1,
                          /*denoising_on=*/false, /*frame_dropper_on=*/false,
                          /*spatial_resize_on=*/false,
                          /*width=*/352, /*height=*/288);
  return config;
}

}  // namespace

TEST(VideoCodecTestFixtureImplTest, CreateWithConfig) {
  auto fixture = std::make_unique<VideoCodecTestFixtureImpl>(CreateConfig());
  fixture->RunTest({kRateProfile}, nullptr, nullptr, nullptr);

  VideoCodecTestStats& stats = fixture->GetStats();
  EXPECT_EQ(kNumFrames, stats.GetFrameStatistics().size());
}

TEST(VideoCodecTestFixtureImplTest, CreateWithFactories) {
  std::unique_ptr<VideoEncoderFactory> encoder_factory =
      CreateBuiltinVideoEncoderFactory();
  std::unique_ptr<VideoDecoderFactory> decoder_factory =
      CreateBuiltinVideoDecoderFactory();

  auto fixture = std::make_unique<VideoCodecTestFixtureImpl>(
      CreateConfig(), std::move(decoder_factory), std::move(encoder_factory));
  fixture->RunTest({kRateProfile}, nullptr, nullptr, nullptr);

  VideoCodecTestStats& stats = fixture->GetStats();
  EXPECT_EQ(kNumFrames, stats.GetFrameStatistics().size());
}

TEST(VideoCodecTestFixtureImplTest, CreateWithCodecs) {
  std::unique_ptr<VideoEncoder> encoder = VP8Encoder::Create();
  std::vector<std::unique_ptr<VideoDecoder>> decoders;
  decoders.push_back(VP8Decoder::Create());

  auto fixture = std::make_unique<VideoCodecTestFixtureImpl>(
      CreateConfig(), std::move(decoders), std::move(encoder));
  fixture->RunTest({kRateProfile}, nullptr, nullptr, nullptr);

  VideoCodecTestStats& stats = fixture->GetStats();
  EXPECT_EQ(kNumFrames, stats.GetFrameStatistics().size());
}

}  // namespace test
}  // namespace webrtc
