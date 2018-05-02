/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

#include <vector>

#include "api/test/create_videoprocessor_integrationtest_fixture.h"
#include "media/base/mediaconstants.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

#if defined(WEBRTC_USE_H264)

namespace {

typedef VideoProcessorIntegrationTestFixtureInterface BaseTest;

// Codec settings.
const int kCifWidth = 352;
const int kCifHeight = 288;
const int kNumFrames = 100;

static TestConfig CreateTestConfig() {
  TestConfig config;
  config.filename = "foreman_cif";
  config.filepath = ResourcePath(config.filename, "yuv");
  config.num_frames = kNumFrames;
  // Only allow encoder/decoder to use single core, for predictability.
  config.use_single_core = true;
  config.hw_encoder = false;
  config.hw_decoder = false;
  config.encoded_frame_checker =
      new VideoProcessorIntegrationTest::H264KeyframeChecker();
  return config;
}

static std::unique_ptr<BaseTest> CreateTestFixtureWithConfig(
    TestConfig config) {
  return CreateVideoProcessorIntegrationTestFixture(config);
}
}  // namespace

TEST(VideoProcessorIntegrationTestOpenH264, ConstantHighBitrate) {
  auto config = CreateTestConfig();
  config.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false, true, false,
                          kCifWidth, kCifHeight);
  auto fixture = CreateTestFixtureWithConfig(config);

  std::vector<RateProfile> rate_profiles = {{500, 30, kNumFrames}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 0, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{37, 35, 0.93, 0.91}};

  fixture->ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                                       &quality_thresholds, nullptr, nullptr);
}

// H264: Enable SingleNalUnit packetization mode. Encoder should split
// large frames into multiple slices and limit length of NAL units.
TEST(VideoProcessorIntegrationTestOpenH264, SingleNalUnit) {
  auto config = CreateTestConfig();
  config.h264_codec_settings.packetization_mode =
      H264PacketizationMode::SingleNalUnit;
  config.max_payload_size_bytes = 500;
  config.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false, true, false,
                          kCifWidth, kCifHeight);
  auto fixture = CreateTestFixtureWithConfig(config);

  std::vector<RateProfile> rate_profiles = {{500, 30, kNumFrames}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 0, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{37, 35, 0.93, 0.91}};

  BitstreamThresholds bs_thresholds = {fixture->config.max_payload_size_bytes};

  fixture->ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                                       &quality_thresholds, &bs_thresholds,
                                       nullptr);
}

#endif  // defined(WEBRTC_USE_H264)

}  // namespace test
}  // namespace webrtc
