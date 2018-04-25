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

#include "api/test/create_videoprocessor_integrationtest_fixture.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {

// Loop variables.
const size_t kBitrates[] = {500};
const VideoCodecType kVideoCodecType[] = {kVideoCodecVP8};
const bool kHwCodec[] = {false};

// Codec settings.
const int kNumSpatialLayers = 1;
const int kNumTemporalLayers = 1;
const bool kDenoisingOn = false;
const bool kSpatialResizeOn = false;
const bool kFrameDropperOn = false;

// Test settings.
const bool kUseSingleCore = false;
const bool kMeasureCpu = false;
const VisualizationParams kVisualizationParams = {
    false,  // save_encoded_ivf
    false,  // save_decoded_y4m
};

const int kNumFrames = 30;

}  // namespace

// Tests for plotting statistics from logs.
class VideoProcessorIntegrationTestParameterized
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          ::testing::tuple<size_t, VideoCodecType, bool>> {
 protected:
  VideoProcessorIntegrationTestParameterized()
      : bitrate_(::testing::get<0>(GetParam())),
        codec_type_(::testing::get<1>(GetParam())),
        hw_codec_(::testing::get<2>(GetParam())) {
    fixture_ = CreateVideoProcessorIntegrationTestFixture();
  }
  ~VideoProcessorIntegrationTestParameterized() override = default;

  void RunTest(size_t width,
               size_t height,
               size_t framerate,
               const std::string& filename) {
    fixture_->config.filename = filename;
    fixture_->config.filepath = ResourcePath(filename, "yuv");
    fixture_->config.use_single_core = kUseSingleCore;
    fixture_->config.measure_cpu = kMeasureCpu;
    fixture_->config.hw_encoder = hw_codec_;
    fixture_->config.hw_decoder = hw_codec_;
    fixture_->config.num_frames = kNumFrames;

    const size_t num_simulcast_streams =
        codec_type_ == kVideoCodecVP8 ? kNumSpatialLayers : 1;
    const size_t num_spatial_layers =
        codec_type_ == kVideoCodecVP9 ? kNumSpatialLayers : 1;

    const std::string codec_name = CodecTypeToPayloadString(codec_type_);
    fixture_->config.SetCodecSettings(codec_name, num_simulcast_streams,
                                      num_spatial_layers, kNumTemporalLayers,
                                      kDenoisingOn, kFrameDropperOn,
                                      kSpatialResizeOn, width, height);

    std::vector<RateProfile> rate_profiles = {
        {bitrate_, framerate, kNumFrames}};

    fixture_->ProcessFramesAndMaybeVerify(rate_profiles, nullptr, nullptr,
                                          nullptr, &kVisualizationParams);
  }
  std::unique_ptr<VideoProcessorIntegrationTestFixtureInterface> fixture_;
  const size_t bitrate_;
  const VideoCodecType codec_type_;
  const bool hw_codec_;
};

INSTANTIATE_TEST_CASE_P(CodecSettings,
                        VideoProcessorIntegrationTestParameterized,
                        ::testing::Combine(::testing::ValuesIn(kBitrates),
                                           ::testing::ValuesIn(kVideoCodecType),
                                           ::testing::ValuesIn(kHwCodec)));

TEST_P(VideoProcessorIntegrationTestParameterized, Foreman_352x288_30) {
  RunTest(352, 288, 30, "foreman_cif");
}

TEST_P(VideoProcessorIntegrationTestParameterized,
       DISABLED_FourPeople_1280x720_30) {
  RunTest(1280, 720, 30, "FourPeople_1280x720_30");
}

}  // namespace test
}  // namespace webrtc
