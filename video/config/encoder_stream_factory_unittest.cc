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

#include <tuple>

#include "api/video_codecs/scalability_mode.h"
#include "call/adaptation/video_source_restrictions.h"
#include "test/explicit_key_value_config.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using ::testing::Combine;
using ::testing::Values;

using ::cricket::EncoderStreamFactory;
using test::ExplicitKeyValueConfig;

class TestVideoStream : public VideoStream {
 public:
  TestVideoStream& WithWidth(int width) {
    this->width = width;
    return *this;
  }
  TestVideoStream& WithHeight(int height) {
    this->height = height;
    return *this;
  }
  TestVideoStream& WithMaxFramerateFps(int max_framerate_fps) {
    this->max_framerate = max_framerate_fps;
    return *this;
  }
  TestVideoStream& WithMinBitrateBps(int min_bitrate_bps) {
    this->min_bitrate_bps = min_bitrate_bps;
    return *this;
  }
  TestVideoStream& WithTargetBitrateBps(int target_bitrate_bps) {
    this->target_bitrate_bps = target_bitrate_bps;
    return *this;
  }
  TestVideoStream& WithMaxBitrateBps(int max_bitrate_bps) {
    this->max_bitrate_bps = max_bitrate_bps;
    return *this;
  }
  TestVideoStream& WithScaleResolutionDownBy(double scale_resolution_down_by) {
    this->scale_resolution_down_by = scale_resolution_down_by;
    return *this;
  }
  TestVideoStream& WithScalabilityMode(
      std::optional<ScalabilityMode> scalability_mode) {
    this->scalability_mode = scalability_mode;
    return *this;
  }
  TestVideoStream& WithRequestedResolution(Resolution requested_resolution) {
    this->requested_resolution = requested_resolution;
    return *this;
  }
};

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
      TestVideoStream().WithRequestedResolution({.width = 640, .height = 360}));
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
      TestVideoStream().WithRequestedResolution({.width = 640, .height = 360}));
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
  Resolution input_resolution;
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
                           test_params.input_resolution, encoder_config);
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
            .input_resolution = {.width = 516, .height = 516},
            .expected_resolution = {.width = 516, .height = 516}},
        ResolutionAlignmentTestParams{
            .number_of_streams = 2,
            .input_resolution = {.width = 515, .height = 517},
            .expected_resolution = {.width = 514, .height = 516}},
        // Custom alignment. Request divisibility by 2.
        ResolutionAlignmentTestParams{
            .field_trials = "WebRTC-NormalizeSimulcastResolution/Enabled-1/",
            .number_of_streams = 2,
            .input_resolution = {.width = 515, .height = 517},
            .expected_resolution = {.width = 514, .height = 516}},
        // Custom alignment. Request divisibiity by 4.
        ResolutionAlignmentTestParams{
            .field_trials = "WebRTC-NormalizeSimulcastResolution/Enabled-2/",
            .number_of_streams = 2,
            .input_resolution = {.width = 515, .height = 517},
            .expected_resolution = {.width = 512, .height = 516}}));

struct LimitStreamCountTestParams {
  std::string field_trials;
  Resolution input_resolution;
  bool is_legacy_screencast = false;
  size_t first_active_layer_idx = 0;
  size_t requested_stream_count = 0;
  size_t expected_stream_count = 0;
};

class EncoderStreamFactoryLimitStreamCountTest
    : public ::testing::TestWithParam<LimitStreamCountTestParams> {};

TEST_P(EncoderStreamFactoryLimitStreamCountTest, LimitStreamCount) {
  LimitStreamCountTestParams test_params = GetParam();
  VideoEncoderConfig encoder_config;
  encoder_config.codec_type = VideoCodecType::kVideoCodecVP8;
  encoder_config.number_of_streams = test_params.requested_stream_count;
  encoder_config.simulcast_layers.resize(test_params.requested_stream_count);
  for (size_t i = 0; i < encoder_config.number_of_streams; ++i) {
    encoder_config.simulcast_layers[i].active =
        (i >= test_params.first_active_layer_idx);
  }
  if (test_params.is_legacy_screencast) {
    encoder_config.content_type = VideoEncoderConfig::ContentType::kScreen;
    encoder_config.legacy_conference_mode = true;
  }
  auto streams =
      CreateEncoderStreams(ExplicitKeyValueConfig(test_params.field_trials),
                           test_params.input_resolution, encoder_config);
  EXPECT_EQ(streams.size(), test_params.expected_stream_count);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    EncoderStreamFactoryLimitStreamCountTest,
    Values(
        // Simulcast stream count is capped based on resolution when
        // WebRTC-LegacySimulcastLayerLimit is not disabled (default).
        LimitStreamCountTestParams{
            .input_resolution = {.width = 1000, .height = 1000},
            .requested_stream_count = 3,
            .expected_stream_count = 3},
        LimitStreamCountTestParams{
            .input_resolution = {.width = 100, .height = 100},
            .requested_stream_count = 3,
            .expected_stream_count = 1},
        // Maximum simulcast stream count in legacy screencast is 2 and is not
        // limited based on resolution.
        LimitStreamCountTestParams{
            .input_resolution = {.width = 100, .height = 100},
            .is_legacy_screencast = true,
            .requested_stream_count = 3,
            .expected_stream_count = 2},
        // WebRTC-LegacySimulcastLayerLimit is disabled. Stream count is not
        // limited based on resolution.
        LimitStreamCountTestParams{
            .field_trials = "WebRTC-LegacySimulcastLayerLimit/Disabled/",
            .input_resolution = {.width = 100, .height = 100},
            .requested_stream_count = 3,
            .expected_stream_count = 3},
        // Streams up to the first active one, inclusive, are always included in
        // the reduced stream set.
        LimitStreamCountTestParams{
            .input_resolution = {.width = 100, .height = 100},
            .first_active_layer_idx = 1,
            .requested_stream_count = 3,
            .expected_stream_count = 2}));

struct OverrideStreamSettingsTestParams {
  std::string field_trials;
  Resolution input_resolution;
  VideoEncoderConfig::ContentType content_type;
  std::vector<VideoStream> requested_streams;
  std::vector<VideoStream> expected_streams;
};

class EncoderStreamFactoryOverrideStreamSettinsTest
    : public ::testing::TestWithParam<
          std::tuple<OverrideStreamSettingsTestParams, VideoCodecType>> {};

TEST_P(EncoderStreamFactoryOverrideStreamSettinsTest, OverrideStreamSettings) {
  OverrideStreamSettingsTestParams test_params = std::get<0>(GetParam());
  VideoEncoderConfig encoder_config;
  encoder_config.codec_type = std::get<1>(GetParam());
  encoder_config.number_of_streams = test_params.requested_streams.size();
  encoder_config.simulcast_layers.resize(encoder_config.number_of_streams);
  for (size_t i = 0; i < encoder_config.number_of_streams; ++i) {
    const VideoStream& src = test_params.requested_streams[i];
    VideoStream& dst = encoder_config.simulcast_layers[i];
    if (src.max_framerate > 0) {
      dst.max_framerate = src.max_framerate;
    }
    if (src.max_bitrate_bps > 0) {
      dst.max_bitrate_bps = src.max_bitrate_bps;
    }
    if (src.scale_resolution_down_by > 0) {
      dst.scale_resolution_down_by = src.scale_resolution_down_by;
    }
    dst.scalability_mode = src.scalability_mode;
  }
  encoder_config.content_type = test_params.content_type;
  auto streams =
      CreateEncoderStreams(ExplicitKeyValueConfig(test_params.field_trials),
                           test_params.input_resolution, encoder_config);
  ASSERT_EQ(streams.size(), test_params.expected_streams.size());
  for (size_t i = 0; i < streams.size(); ++i) {
    const webrtc::VideoStream& expected = test_params.expected_streams[i];
    EXPECT_EQ(streams[i].width, expected.width);
    EXPECT_EQ(streams[i].height, expected.height);
    EXPECT_EQ(streams[i].max_framerate, expected.max_framerate);
    EXPECT_EQ(streams[i].min_bitrate_bps, expected.min_bitrate_bps);
    EXPECT_EQ(streams[i].target_bitrate_bps, expected.target_bitrate_bps);
    EXPECT_EQ(streams[i].max_bitrate_bps, expected.max_bitrate_bps);
    EXPECT_EQ(streams[i].scalability_mode, expected.scalability_mode);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Screencast,
    EncoderStreamFactoryOverrideStreamSettinsTest,
    Combine(Values(OverrideStreamSettingsTestParams{
                .input_resolution = {.width = 1920, .height = 1080},
                .content_type = VideoEncoderConfig::ContentType::kScreen,
                .requested_streams =
                    {TestVideoStream()
                         .WithMaxFramerateFps(5)
                         .WithMaxBitrateBps(420'000)
                         .WithScaleResolutionDownBy(1)
                         .WithScalabilityMode(ScalabilityMode::kL1T2),
                     TestVideoStream()
                         .WithMaxFramerateFps(30)
                         .WithMaxBitrateBps(2500'000)
                         .WithScaleResolutionDownBy(1)
                         .WithScalabilityMode(ScalabilityMode::kL1T2)},
                .expected_streams =
                    {TestVideoStream()
                         .WithWidth(1920)
                         .WithHeight(1080)
                         .WithMaxFramerateFps(5)
                         .WithMinBitrateBps(30'000)
                         .WithTargetBitrateBps(420'000)
                         .WithMaxBitrateBps(420'000)
                         .WithScalabilityMode(ScalabilityMode::kL1T2),
                     TestVideoStream()
                         .WithWidth(1920)
                         .WithHeight(1080)
                         .WithMaxFramerateFps(30)
                         .WithMinBitrateBps(800'000)
                         .WithTargetBitrateBps(2500'000)
                         .WithMaxBitrateBps(2500'000)
                         .WithScalabilityMode(ScalabilityMode::kL1T2)}}),
            Values(VideoCodecType::kVideoCodecVP8,
                   VideoCodecType::kVideoCodecAV1)));

}  // namespace webrtc
