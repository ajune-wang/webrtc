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
#include "test/gtest.h"
#include "api/test/video/function_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/media_constants.h"
#include "media/engine/internal_decoder_factory.h"
#include "media/engine/internal_encoder_factory.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"


namespace webrtc {
namespace test {

namespace {
// const size_t kBitratesKbps[] = {50, 100, 200, 400, 800};
const size_t kBitratesKbps[] = {200};

const size_t kWidth = 320;
const size_t kHeight = 240;
const size_t kFrameRateFps = 30;

VideoCodecTestFixture::Config CreateTestConfig() {
  VideoCodecTestFixture::Config config;
  // config.filename = "Bridge_640x360_30";
  // config.filename = "Street_640x360_30";
  // config.filepath = ResourcePath(config.filename, "yuv");
  config.num_frames = 900;

  // Special settings for measuring encode speed [fps].
  config.use_single_core = true;
  config.measure_cpu = false;
  // config.decode = false;
  return config;
}

void RunTest(const VideoCodecTestFixture::Config& config,
             VideoCodecTestFixture* fixture) {
  std::map<size_t, std::vector<VideoCodecTestStats::VideoStatistics>> rd_stats;

  for (size_t bitrate_kbps : kBitratesKbps) {
    const std::vector<RateProfile> rate_profiles = {
        {bitrate_kbps, kFrameRateFps, 0}};

    fixture->RunTest(rate_profiles, nullptr, nullptr, nullptr);

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

TEST(VideoCodecTestArmEval, LibvpxVp8_SingleLayer_DenoisingOff_Room) {
  auto config = CreateTestConfig();
  config.filename = "Room.320_240";
  config.filepath = ResourcePath(config.filename, "yuv");
  config.SetCodecSettings(cricket::kVp8CodecName, 1, 1, 1,
                          false /* denoising_on */,
                          false /* frame_dropper_on */,
                          false /* spatial_resize_on */, kWidth, kHeight);
  auto fixture = CreateVideoCodecTestFixture(config);
  RunTest(config, fixture.get());
}

TEST(VideoCodecTestArmEval, LibvpxVp8_SingleLayer_DenoisingOff_Bridge) {
  auto config = CreateTestConfig();
  config.filename = "Bridge.320_240";
  config.filepath = ResourcePath(config.filename, "yuv");
  config.SetCodecSettings(cricket::kVp8CodecName, 1, 1, 1,
                          false /* denoising_on */,
                          false /* frame_dropper_on */,
                          false /* spatial_resize_on */, kWidth, kHeight);
  auto fixture = CreateVideoCodecTestFixture(config);
  RunTest(config, fixture.get());
}

TEST(VideoCodecTestArmEval, LibvpxVp8_SingleLayer_DenoisingOff_Street) {
  auto config = CreateTestConfig();
  config.filename = "Street.320_240";
  config.filepath = ResourcePath(config.filename, "yuv");
  config.SetCodecSettings(cricket::kVp8CodecName, 1, 1, 1,
                          false /* denoising_on */,
                          false /* frame_dropper_on */,
                          false /* spatial_resize_on */, kWidth, kHeight);
  auto fixture = CreateVideoCodecTestFixture(config);
  RunTest(config, fixture.get());
}

TEST(VideoCodecTestArmEval, LibvpxVp9_SingleLayer_DenoisingOff_Room) {
  auto config = CreateTestConfig();
  config.filename = "Room.320_240";
  config.filepath = ResourcePath(config.filename, "yuv");
  config.SetCodecSettings(cricket::kVp9CodecName, 1, 1, 1,
                          false /* denoising_on */,
                          false /* frame_dropper_on */,
                          false /* spatial_resize_on */, kWidth, kHeight);
  auto fixture = CreateVideoCodecTestFixture(config);
  RunTest(config, fixture.get());
}

TEST(VideoCodecTestArmEval, LibvpxVp9_SingleLayer_DenoisingOff_Street) {
  auto config = CreateTestConfig();
  config.filename = "Street.320_240";
  config.filepath = ResourcePath(config.filename, "yuv");
  config.SetCodecSettings(cricket::kVp9CodecName, 1, 1, 1,
                          false /* denoising_on */,
                          false /* frame_dropper_on */,
                          false /* spatial_resize_on */, kWidth, kHeight);
  auto fixture = CreateVideoCodecTestFixture(config);
  RunTest(config, fixture.get());
}

TEST(VideoCodecTestArmEval, LibvpxVp9_SingleLayer_DenoisingOff_Bridge) {
  auto config = CreateTestConfig();
  config.filename = "Bridge.320_240";
  config.filepath = ResourcePath(config.filename, "yuv");
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
