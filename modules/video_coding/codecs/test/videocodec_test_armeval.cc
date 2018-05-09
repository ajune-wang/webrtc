/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
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
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {
const size_t kBitratesKbps[] = {50, 100, 200, 400, 800};

const size_t kWidth = 640;
const size_t kHeight = 360;
const size_t kFrameRateFps = 30;

const VisualizationParams kVisualizationParams = {
    false,  // save_encoded_ivf
    false,  // save_decoded_y4m
};

TestConfig CreateTestConfig() {
  TestConfig config;
  // config.filename = "Room_640x360_30";
  config.filename = "Bridge_640x360_30";
  // config.filename = "Street_640x360_30";
  config.filepath = ResourcePath(config.filename, "yuv");
  config.num_frames = 300;

  // Special settings for measuring encode speed [fps].
  config.use_single_core = true;
  config.measure_cpu = false;
  config.decode = false;
  return config;
}

void RunTest(const TestConfig& config, VideoCodecTestFixture* fixture) {
  std::map<size_t, std::vector<VideoStatistics>> rd_stats;

  for (size_t bitrate_kbps : kBitratesKbps) {
    const std::vector<RateProfile> rate_profiles = {
        {bitrate_kbps, kFrameRateFps, config.num_frames}};

    fixture->RunTest(rate_profiles, nullptr, nullptr, nullptr,
                     &kVisualizationParams);

    rd_stats[bitrate_kbps] =
        fixture->GetStats().SliceAndCalcLayerVideoStatistic(
            0, config.num_frames - 1);
  }

  printf("--> Summary\n");
  printf("%13s %7s %7s %13s %13s %7s %13s %13s\n", "uplink_kbps", "width",
         "height", "downlink_kbps", "framerate_fps", "psnr", "enc_speed_fps",
         "dec_speed_fps");
  for (const auto& rd_stat : rd_stats) {
    const size_t bitrate_kbps = rd_stat.first;
    for (const auto& layer_stat : rd_stat.second) {
      printf("%13zu %7zu %7zu %13zu %13.2f %7.2f %13.2f %13.2f\n", bitrate_kbps,
             layer_stat.width, layer_stat.height, layer_stat.bitrate_kbps,
             layer_stat.framerate_fps, layer_stat.avg_psnr,
             layer_stat.enc_speed_fps, layer_stat.dec_speed_fps);
    }
  }
}

}  // namespace

TEST(VideoCodecTestArmEval, LibvpxVp8_SingleLayer_DenoisingOff) {
  auto config = CreateTestConfig();
  config.SetCodecSettings(cricket::kVp8CodecName, 1, 1, 1,
                          false /* denoising_on */,
                          false /* frame_dropper_on */,
                          false /* spatial_resize_on */, kWidth, kHeight);
  auto fixture = CreateVideoCodecTestFixture(config);
  RunTest(config, fixture.get());
}

TEST(VideoCodecTestArmEval, LibvpxVp9_SingleLayer_DenoisingOff) {
  auto config = CreateTestConfig();
  config.SetCodecSettings(cricket::kVp9CodecName, 1, 1, 1,
                          false /* denoising_on */,
                          false /* frame_dropper_on */,
                          false /* spatial_resize_on */, kWidth, kHeight);
  auto fixture = CreateVideoCodecTestFixture(config);
  RunTest(config, fixture.get());
}

TEST(VideoCodecTestArmEval, LibvpxVp8_SingleLayer_DenoisingOn) {
  auto config = CreateTestConfig();
  config.SetCodecSettings(cricket::kVp8CodecName, 1, 1, 1,
                          true /* denoising_on */, false /* frame_dropper_on */,
                          false /* spatial_resize_on */, kWidth, kHeight);
  auto fixture = CreateVideoCodecTestFixture(config);
  RunTest(config, fixture.get());
}

TEST(VideoCodecTestArmEval, LibvpxVp9_SingleLayer_DenoisingOn) {
  auto config = CreateTestConfig();
  config.SetCodecSettings(cricket::kVp9CodecName, 1, 1, 1,
                          true /* denoising_on */, false /* frame_dropper_on */,
                          false /* spatial_resize_on */, kWidth, kHeight);
  auto fixture = CreateVideoCodecTestFixture(config);
  RunTest(config, fixture.get());
}

TEST(VideoCodecTestArmEval, LibvpxVp8_Multires_2SL3TL) {
  auto config = CreateTestConfig();
  config.SetCodecSettings(cricket::kVp8CodecName, 2, 1, 3,
                          false /* denoising_on */,
                          false /* frame_dropper_on */,
                          false /* spatial_resize_on */, kWidth, kHeight);
  auto fixture = CreateVideoCodecTestFixture(config);
  RunTest(config, fixture.get());
}

TEST(VideoCodecTestArmEval, LibvpxVp9_Svc_2SL3TL) {
  auto config = CreateTestConfig();
  config.SetCodecSettings(cricket::kVp9CodecName, 1, 2, 3,
                          false /* denoising_on */,
                          false /* frame_dropper_on */,
                          false /* spatial_resize_on */, kWidth, kHeight);
  auto fixture = CreateVideoCodecTestFixture(config);
  RunTest(config, fixture.get());
}

}  // namespace test
}  // namespace webrtc
