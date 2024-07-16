/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/config/encoder_stream_factory.h"

#include "call/adaptation/video_source_restrictions.h"
#include "test/explicit_key_value_config.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using ::testing::Values;

using ::cricket::EncoderStreamFactory;
using test::ExplicitKeyValueConfig;

std::vector<Resolution> GetStreamResolutions(
    const std::vector<VideoStream>& streams) {
  std::vector<Resolution> res;
  for (const auto& s : streams) {
    if (s.active) {
      res.push_back(
          {rtc::checked_cast<int>(s.width), rtc::checked_cast<int>(s.height)});
    }
  }
  return res;
}

VideoStream LayerWithRequestedResolution(Resolution res) {
  VideoStream s;
  s.requested_resolution = res;
  return s;
}

std::vector<VideoStream> CreateEncoderStreams(
    const FieldTrialsView& field_trials,
    const Resolution& resolution,
    const webrtc::VideoEncoderConfig& encoder_config,
    absl::optional<VideoSourceRestrictions> restrictions = absl::nullopt) {
  VideoEncoder::EncoderInfo encoder_info;
  auto factory =
      rtc::make_ref_counted<EncoderStreamFactory>(encoder_info, restrictions);
  return factory->CreateEncoderStreams(field_trials, resolution.width,
                                       resolution.height, encoder_config);
}

}  // namespace

TEST(EncoderStreamFactory, SinglecastRequestedResolution) {
  ExplicitKeyValueConfig field_trials("");
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 1;
  encoder_config.simulcast_layers.push_back(
      LayerWithRequestedResolution({.width = 640, .height = 360}));
  auto streams = CreateEncoderStreams(
      field_trials, /*resolution=*/{.width = 1280, .height = 720},
      encoder_config);
  EXPECT_EQ(streams[0].requested_resolution,
            (Resolution{.width = 640, .height = 360}));
  EXPECT_EQ(GetStreamResolutions(streams), (std::vector<Resolution>{
                                               {.width = 640, .height = 360},
                                           }));
}

TEST(EncoderStreamFactory, SinglecastRequestedResolutionWithAdaptation) {
  ExplicitKeyValueConfig field_trials("");
  VideoSourceRestrictions restrictions(
      /* max_pixels_per_frame= */ (320 * 320),
      /* target_pixels_per_frame= */ absl::nullopt,
      /* max_frame_rate= */ absl::nullopt);
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 1;
  encoder_config.simulcast_layers.push_back(
      LayerWithRequestedResolution({.width = 640, .height = 360}));
  auto streams = CreateEncoderStreams(
      field_trials, /*resolution=*/{.width = 1280, .height = 720},
      encoder_config, restrictions);
  EXPECT_EQ(streams[0].requested_resolution,
            (Resolution{.width = 640, .height = 360}));
  EXPECT_EQ(GetStreamResolutions(streams), (std::vector<Resolution>{
                                               {.width = 320, .height = 180},
                                           }));
}

TEST(EncoderStreamFactory, BitratePriority) {
  constexpr double kBitratePriority = 0.123;
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 2;
  encoder_config.simulcast_layers.resize(encoder_config.number_of_streams);
  encoder_config.bitrate_priority = kBitratePriority;
  auto streams = CreateEncoderStreams(
      /*field_trials=*/ExplicitKeyValueConfig(""),
      /*resolution=*/{.width = 640, .height = 360}, encoder_config);
  ASSERT_EQ(streams.size(), 2u);
  EXPECT_EQ(streams[0].bitrate_priority, kBitratePriority);
  EXPECT_FALSE(streams[1].bitrate_priority);
}

struct ResolutionAlignmentTestParams {
  std::string field_trials;
  size_t number_of_streams;
  Resolution resolution;
  Resolution expected_resolution;
};

class EncoderStreamFactoryResolutionAlignmentTest
    : public ::testing::TestWithParam<ResolutionAlignmentTestParams> {};

TEST_P(EncoderStreamFactoryResolutionAlignmentTest, ResolutionAlignment) {
  ResolutionAlignmentTestParams test_params = GetParam();
  VideoEncoderConfig encoder_config;
  encoder_config.codec_type = VideoCodecType::kVideoCodecVP8;
  encoder_config.number_of_streams = test_params.number_of_streams;
  encoder_config.simulcast_layers.resize(test_params.number_of_streams);
  auto streams =
      CreateEncoderStreams(ExplicitKeyValueConfig(test_params.field_trials),
                           test_params.resolution, encoder_config);
  ASSERT_EQ(streams.size(), test_params.number_of_streams);
  EXPECT_EQ(GetStreamResolutions(streams).back(),
            test_params.expected_resolution);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    EncoderStreamFactoryResolutionAlignmentTest,
    Values(
        // Default alignment. Resolution of the largest stream must be divisible
        // by (2 ^ (number_of_streams - 1)). Use high enough resolution to avoid
        // reduction of stream count.
        ResolutionAlignmentTestParams{
            .number_of_streams = 2,
            .resolution = {.width = 516, .height = 516},
            .expected_resolution = {.width = 516, .height = 516}},
        ResolutionAlignmentTestParams{
            .number_of_streams = 2,
            .resolution = {.width = 515, .height = 517},
            .expected_resolution = {.width = 514, .height = 516}},
        // Custom alignment. Request divisibility by 2.
        ResolutionAlignmentTestParams{
            .field_trials = "WebRTC-NormalizeSimulcastResolution/Enabled-1/",
            .number_of_streams = 2,
            .resolution = {.width = 515, .height = 517},
            .expected_resolution = {.width = 514, .height = 516}},
        // Custom alignment. Request divisibiity by 4.
        ResolutionAlignmentTestParams{
            .field_trials = "WebRTC-NormalizeSimulcastResolution/Enabled-2/",
            .number_of_streams = 2,
            .resolution = {.width = 515, .height = 517},
            .expected_resolution = {.width = 512, .height = 516}}));

struct LimitStreamCountTestParams {
  std::string field_trials;
  Resolution resolution;
  bool is_legacy_screencast;
  size_t requested_stream_count;
  size_t expected_stream_count;
};

class EncoderStreamFactoryLimitStreamCountTest
    : public ::testing::TestWithParam<LimitStreamCountTestParams> {};

TEST_P(EncoderStreamFactoryLimitStreamCountTest, LimitStreamCount) {
  LimitStreamCountTestParams test_params = GetParam();
  VideoEncoderConfig encoder_config;
  encoder_config.codec_type = VideoCodecType::kVideoCodecVP8;
  encoder_config.number_of_streams = test_params.requested_stream_count;
  encoder_config.simulcast_layers.resize(test_params.requested_stream_count);
  if (test_params.is_legacy_screencast) {
    encoder_config.content_type = VideoEncoderConfig::ContentType::kScreen;
    encoder_config.legacy_conference_mode = true;
  }
  auto streams =
      CreateEncoderStreams(ExplicitKeyValueConfig(test_params.field_trials),
                           test_params.resolution, encoder_config);
  EXPECT_EQ(streams.size(), test_params.expected_stream_count);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    EncoderStreamFactoryLimitStreamCountTest,
    Values(
        // Simulcast stream count is capped based on resolution when
        // WebRTC-LegacySimulcastLayerLimit is enabled.
        LimitStreamCountTestParams{
            .field_trials = "WebRTC-LegacySimulcastLayerLimit/Enabled/",
            .resolution = {.width = 1000, .height = 1000},
            .requested_stream_count = 3,
            .expected_stream_count = 3},
        LimitStreamCountTestParams{
            .field_trials = "WebRTC-LegacySimulcastLayerLimit/Enabled/",
            .resolution = {.width = 100, .height = 100},
            .requested_stream_count = 3,
            .expected_stream_count = 1},
        // Maximum simulcast stream count in legacy screencast is 2 and is not a
        // factor of WebRTC-LegacySimulcastLayerLimit and resolution.
        LimitStreamCountTestParams{
            .field_trials = "WebRTC-LegacySimulcastLayerLimit/Enabled/",
            .resolution = {.width = 100, .height = 100},
            .is_legacy_screencast = true,
            .requested_stream_count = 3,
            .expected_stream_count = 2},
        // No resolution-based capping of simulcast stream count when
        // WebRTC-LegacySimulcastLayerLimit is disabled.
        LimitStreamCountTestParams{
            .field_trials = "WebRTC-LegacySimulcastLayerLimit/Disabled/",
            .resolution = {.width = 100, .height = 100},
            .requested_stream_count = 3,
            .expected_stream_count = 3}));

}  // namespace webrtc
