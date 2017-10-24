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

#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {

// Loop variables.
const int kBitrates[] = {10,  20,  30,  40,  50,  60,  70,  80,  90,  100,
                         125, 150, 175, 200, 225, 250, 275, 300, 400, 500};
const VideoCodecType kVideoCodecType[] = {kVideoCodecVP9};
const bool kHwCodec[] = {false, true};

// Codec settings.
const bool kResilienceOn = false;
const int kNumTemporalLayers = 1;
const bool kDenoisingOn = false;
const bool kErrorConcealmentOn = false;
const bool kSpatialResizeOn = false;
const bool kFrameDropperOn = false;

// Test settings.
const bool kUseSingleCore = false;
const bool kMeasureCpu = false;
const VisualizationParams kVisualizationParams = {
    false,  // save_encoded_ivf
    false,  // save_decoded_y4m
};

const int kClipLengthSeconds = 30;

}  // namespace

// Tests for plotting statistics from logs.
class VideoProcessorIntegrationTestParameterized
    : public VideoProcessorIntegrationTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<int, VideoCodecType, bool>> {
 protected:
  VideoProcessorIntegrationTestParameterized()
      : bitrate_(::testing::get<0>(GetParam())),
        codec_type_(::testing::get<1>(GetParam())),
        hw_codec_(::testing::get<2>(GetParam())) {}
  ~VideoProcessorIntegrationTestParameterized() override = default;

  void RunTest(int width,
               int height,
               int framerate,
               const std::string& filename) {
    config_.filename = filename;
    config_.input_filename = ResourcePath(filename, "yuv");
    config_.output_filename = TempFilename(
        OutputPath(), "videoprocessor_integrationtest_parameterized");
    config_.use_single_core = kUseSingleCore;
    config_.measure_cpu = kMeasureCpu;
    config_.verbose = true;
    config_.hw_encoder = hw_codec_;
    config_.hw_decoder = hw_codec_;
    config_.num_frames = kClipLengthSeconds * framerate;
    config_.SetCodecSettings(codec_type_, kNumTemporalLayers,
                             kErrorConcealmentOn, kDenoisingOn, kFrameDropperOn,
                             kSpatialResizeOn, kResilienceOn, width, height);

    std::vector<RateProfile> rate_profiles = {
        {bitrate_, framerate, config_.num_frames + 1}};

    ProcessFramesAndMaybeVerify(rate_profiles, nullptr, nullptr, nullptr,
                                &kVisualizationParams);
  }

  const int bitrate_;
  const VideoCodecType codec_type_;
  const bool hw_codec_;
};

INSTANTIATE_TEST_CASE_P(CodecSettings,
                        VideoProcessorIntegrationTestParameterized,
                        ::testing::Combine(::testing::ValuesIn(kBitrates),
                                           ::testing::ValuesIn(kVideoCodecType),
                                           ::testing::ValuesIn(kHwCodec)));

TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r90_f7) {
  RunTest(90, 160, 7, "Bridge_r90_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r136_f7) {
  RunTest(136, 242, 7, "Bridge_r136_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r180_f7) {
  RunTest(180, 320, 7, "Bridge_r180_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r270_f7) {
  RunTest(270, 480, 7, "Bridge_r270_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r360_f7) {
  RunTest(360, 640, 7, "Bridge_r360_f7");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r90_f10) {
  RunTest(90, 160, 10, "Bridge_r90_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r136_f10) {
  RunTest(136, 242, 10, "Bridge_r136_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r180_f10) {
  RunTest(180, 320, 10, "Bridge_r180_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r270_f10) {
  RunTest(270, 480, 10, "Bridge_r270_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r360_f10) {
  RunTest(360, 640, 10, "Bridge_r360_f10");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r90_f15) {
  RunTest(90, 160, 15, "Bridge_r90_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r136_f15) {
  RunTest(136, 242, 15, "Bridge_r136_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r180_f15) {
  RunTest(180, 320, 15, "Bridge_r180_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r270_f15) {
  RunTest(270, 480, 15, "Bridge_r270_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Bridge_r360_f15) {
  RunTest(360, 640, 15, "Bridge_r360_f15");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r90_f7) {
  RunTest(90, 160, 7, "Central_Station_r90_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r136_f7) {
  RunTest(136, 242, 7, "Central_Station_r136_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r180_f7) {
  RunTest(180, 320, 7, "Central_Station_r180_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r270_f7) {
  RunTest(270, 480, 7, "Central_Station_r270_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r360_f7) {
  RunTest(360, 640, 7, "Central_Station_r360_f7");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r90_f10) {
  RunTest(90, 160, 10, "Central_Station_r90_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r136_f10) {
  RunTest(136, 242, 10, "Central_Station_r136_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r180_f10) {
  RunTest(180, 320, 10, "Central_Station_r180_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r270_f10) {
  RunTest(270, 480, 10, "Central_Station_r270_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r360_f10) {
  RunTest(360, 640, 10, "Central_Station_r360_f10");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r90_f15) {
  RunTest(90, 160, 15, "Central_Station_r90_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r136_f15) {
  RunTest(136, 242, 15, "Central_Station_r136_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r180_f15) {
  RunTest(180, 320, 15, "Central_Station_r180_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r270_f15) {
  RunTest(270, 480, 15, "Central_Station_r270_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Central_Station_r360_f15) {
  RunTest(360, 640, 15, "Central_Station_r360_f15");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r90_f7) {
  RunTest(90, 160, 7, "Living_Room_r90_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r136_f7) {
  RunTest(136, 242, 7, "Living_Room_r136_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r180_f7) {
  RunTest(180, 320, 7, "Living_Room_r180_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r270_f7) {
  RunTest(270, 480, 7, "Living_Room_r270_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r360_f7) {
  RunTest(360, 640, 7, "Living_Room_r360_f7");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r90_f10) {
  RunTest(90, 160, 10, "Living_Room_r90_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r136_f10) {
  RunTest(136, 242, 10, "Living_Room_r136_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r180_f10) {
  RunTest(180, 320, 10, "Living_Room_r180_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r270_f10) {
  RunTest(270, 480, 10, "Living_Room_r270_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r360_f10) {
  RunTest(360, 640, 10, "Living_Room_r360_f10");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r90_f15) {
  RunTest(90, 160, 15, "Living_Room_r90_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r136_f15) {
  RunTest(136, 242, 15, "Living_Room_r136_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r180_f15) {
  RunTest(180, 320, 15, "Living_Room_r180_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r270_f15) {
  RunTest(270, 480, 15, "Living_Room_r270_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Living_Room_r360_f15) {
  RunTest(360, 640, 15, "Living_Room_r360_f15");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Street_r90_f7) {
  RunTest(90, 160, 7, "Street_r90_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Street_r136_f7) {
  RunTest(136, 242, 7, "Street_r136_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Street_r180_f7) {
  RunTest(180, 320, 7, "Street_r180_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Street_r270_f7) {
  RunTest(270, 480, 7, "Street_r270_f7");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Street_r360_f7) {
  RunTest(360, 640, 7, "Street_r360_f7");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Street_r90_f10) {
  RunTest(90, 160, 10, "Street_r90_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Street_r136_f10) {
  RunTest(136, 242, 10, "Street_r136_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Street_r180_f10) {
  RunTest(180, 320, 10, "Street_r180_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Street_r270_f10) {
  RunTest(270, 480, 10, "Street_r270_f10");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Street_r360_f10) {
  RunTest(360, 640, 10, "Street_r360_f10");
}

TEST_P(VideoProcessorIntegrationTestParameterized, Street_r90_f15) {
  RunTest(90, 160, 15, "Street_r90_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Street_r136_f15) {
  RunTest(136, 242, 15, "Street_r136_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Street_r180_f15) {
  RunTest(180, 320, 15, "Street_r180_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Street_r270_f15) {
  RunTest(270, 480, 15, "Street_r270_f15");
}
TEST_P(VideoProcessorIntegrationTestParameterized, Street_r360_f15) {
  RunTest(360, 640, 15, "Street_r360_f15");
}

}  // namespace test
}  // namespace webrtc
