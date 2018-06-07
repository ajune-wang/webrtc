/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "api/test/create_videocodec_test_fixture.h"
#include "media/base/mediaconstants.h"
#include "modules/video_coding/codecs/test/objc_codec_factory_helper.h"
#include "modules/video_coding/codecs/test/videocodec_test_fixture_impl.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

using VideoStatistics = VideoCodecTestStats::VideoStatistics;

namespace {
const int kForemanNumFrames = 300;
const size_t kBitrateRdPerfKbps[] = {100,  200,  300,  400,  500,  600,
                                     700,  800,  900, 1000};
const size_t kNumFirstFramesToSkipAtRdPerfAnalysis = 60;

VideoCodecTestFixture::Config CreateConfig() {
  VideoCodecTestFixture::Config config;
  config.filename = "foreman_cif";
  config.filepath = ResourcePath(config.filename, "yuv");
  config.num_frames = kForemanNumFrames;
  config.hw_encoder = true;
  config.hw_decoder = true;
  return config;
}

std::unique_ptr<VideoCodecTestFixture> CreateTestFixtureWithConfig(
    VideoCodecTestFixture::Config config) {
  auto decoder_factory = CreateObjCDecoderFactory();
  auto encoder_factory = CreateObjCEncoderFactory();
  return CreateVideoCodecTestFixture(
      config, std::move(decoder_factory), std::move(encoder_factory));
}

void PrintRdPerf(std::map<size_t, std::vector<VideoStatistics>> rd_stats) {
  printf("--> Summary\n");
  printf("%11s %5s %6s %11s %12s %11s %13s %13s %5s %7s %7s %7s %13s %13s\n",
         "uplink_kbps", "width", "height", "spatial_idx", "temporal_idx",
         "target_kbps", "downlink_kbps", "framerate_fps", "psnr", "psnr_y",
         "psnr_u", "psnr_v", "enc_speed_fps", "dec_speed_fps");
  for (const auto& rd_stat : rd_stats) {
    const size_t bitrate_kbps = rd_stat.first;
    for (const auto& layer_stat : rd_stat.second) {
      printf(
          "%11zu %5zu %6zu %11zu %12zu %11zu %13zu %13.2f %5.2f %7.2f %7.2f "
          "%7.2f"
          "%13.2f %13.2f\n",
          bitrate_kbps, layer_stat.width, layer_stat.height,
          layer_stat.spatial_idx, layer_stat.temporal_idx,
          layer_stat.target_bitrate_kbps, layer_stat.bitrate_kbps,
          layer_stat.framerate_fps, layer_stat.avg_psnr, layer_stat.avg_psnr_y,
          layer_stat.avg_psnr_u, layer_stat.avg_psnr_v,
          layer_stat.enc_speed_fps, layer_stat.dec_speed_fps);
    }
  }
}

}  // namespace

// TODO(webrtc:9099): Disabled until the issue is fixed.
// HW codecs don't work on simulators. Only run these tests on device.
// #if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
// #define MAYBE_TEST TEST
// #else
#define MAYBE_TEST(s, name) TEST(s, DISABLED_##name)
// #endif

// TODO(kthelgason): Use RC Thresholds when the internal bitrateAdjuster is no
// longer in use.
MAYBE_TEST(VideoCodecTestVideoToolbox, ForemanCif500kbpsH264CBP) {
  const auto frame_checker = rtc::MakeUnique<
      VideoCodecTestFixtureImpl::H264KeyframeChecker>();
  auto config = CreateConfig();
  config.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false, false, false,
                          352, 288);
  config.encoded_frame_checker = frame_checker.get();
  auto fixture = CreateTestFixtureWithConfig(config);

  std::vector<RateProfile> rate_profiles = {{500, 30, kForemanNumFrames}};

  std::vector<QualityThresholds> quality_thresholds = {{33, 29, 0.9, 0.82}};

  fixture->RunTest(rate_profiles, nullptr, &quality_thresholds, nullptr);
}

MAYBE_TEST(VideoCodecTestVideoToolbox, ForemanCif500kbpsH264CHP) {
  const auto frame_checker = rtc::MakeUnique<
      VideoCodecTestFixtureImpl::H264KeyframeChecker>();
  auto config = CreateConfig();
  config.h264_codec_settings.profile = H264::kProfileConstrainedHigh;
  config.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false, false, false,
                          352, 288);
  config.encoded_frame_checker = frame_checker.get();
  auto fixture = CreateTestFixtureWithConfig(config);

  std::vector<RateProfile> rate_profiles = {{500, 30, kForemanNumFrames}};

  std::vector<QualityThresholds> quality_thresholds = {{33, 30, 0.91, 0.83}};

  fixture->RunTest(rate_profiles, nullptr, &quality_thresholds, nullptr);
}

TEST(VideoCodecTestVideoToolbox, H264RdPerf) {
  auto config = CreateConfig();
  config.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false, false, false,
                          352, 288);
  auto fixture = CreateTestFixtureWithConfig(config);

  std::map<size_t, std::vector<VideoStatistics>> rd_stats;
  for (size_t bitrate_kbps : kBitrateRdPerfKbps) {
    std::vector<RateProfile> rate_profiles = {
        {bitrate_kbps, 30, config.num_frames}};

    fixture->RunTest(rate_profiles, nullptr, nullptr, nullptr);

    rd_stats[bitrate_kbps] =
        fixture->GetStats().SliceAndCalcLayerVideoStatistic(
            kNumFirstFramesToSkipAtRdPerfAnalysis, config.num_frames - 1);
  }

  PrintRdPerf(rd_stats);
}

}  // namespace test
}  // namespace webrtc
