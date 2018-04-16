/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

#include <vector>

#include "media/base/mediaconstants.h"
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

void PrintRdPerf(std::map<size_t, std::vector<VideoStatistics>> rd_stats) {
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

class VideoProcessorIntegrationTestArmEval
    : public VideoProcessorIntegrationTest {
 protected:
  VideoProcessorIntegrationTestArmEval() {
    // config_.filename = "Room_640x360_30";
    config_.filename = "Bridge_640x360_30";
    // config_.filename = "Street_640x360_30";
    config_.filepath = ResourcePath(config_.filename, "yuv");
    config_.num_frames = 300;

    // Special settings for measuring encode speed [fps].
    config_.use_single_core = true;
    config_.measure_cpu = false;
    config_.decode = false;
  }

  void RunTest() {
    std::map<size_t, std::vector<VideoStatistics>> rd_stats;
    for (size_t bitrate_kbps : kBitratesKbps) {
      const std::vector<RateProfile> rate_profiles = {
          {bitrate_kbps, kFrameRateFps, config_.num_frames}};

      ProcessFramesAndMaybeVerify(rate_profiles, nullptr, nullptr, nullptr,
                                  &kVisualizationParams);

      rd_stats[bitrate_kbps] =
          stats_.SliceAndCalcLayerVideoStatistic(0, config_.num_frames - 1);
    }

    PrintRdPerf(rd_stats);
  }
};

TEST_F(VideoProcessorIntegrationTestArmEval, LibvpxVp8_SingleLayer) {
  config_.SetCodecSettings(
      cricket::kVp8CodecName, 1, 1, 1, false /* denoising_on */,
      false /* frame_dropper_on */, false /* spatial_resize_on */,
      false /* resilience_on */, kWidth, kHeight);
  RunTest();
}

TEST_F(VideoProcessorIntegrationTestArmEval, LibvpxVp9_SingleLayer) {
  config_.SetCodecSettings(
      cricket::kVp9CodecName, 1, 1, 1, false /* denoising_on */,
      false /* frame_dropper_on */, false /* spatial_resize_on */,
      false /* resilience_on */, kWidth, kHeight);
  RunTest();
}

TEST_F(VideoProcessorIntegrationTestArmEval, LibvpxVp8_Multires_2SL3TL) {
  config_.SetCodecSettings(
      cricket::kVp8CodecName, 2, 1, 3, false /* denoising_on */,
      false /* frame_dropper_on */, false /* spatial_resize_on */,
      true /* resilience_on */, kWidth, kHeight);
  RunTest();
}

TEST_F(VideoProcessorIntegrationTestArmEval, LibvpxVp9_Svc_2SL3TL) {
  config_.SetCodecSettings(
      cricket::kVp9CodecName, 1, 2, 3, false /* denoising_on */,
      false /* frame_dropper_on */, false /* spatial_resize_on */,
      true /* resilience_on */, kWidth, kHeight);
  RunTest();
}

}  // namespace test
}  // namespace webrtc
