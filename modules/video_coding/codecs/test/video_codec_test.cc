/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_codec.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/functional/any_invocable.h"
#include "api/test/create_video_codec_tester.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "api/test/video_codec_tester.h"
#include "api/test/videocodec_test_stats.h"
#include "api/units/data_rate.h"
#include "api/units/frequency.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/resolution.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "media/engine/internal_decoder_factory.h"
#include "media/engine/internal_encoder_factory.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#if defined(WEBRTC_ANDROID)
#include "modules/video_coding/codecs/test/android_codec_factory_helper.h"
#endif
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

ABSL_FLAG(bool, dump_decoder_input, false, "Dump decoder input.");

ABSL_FLAG(bool, dump_decoder_output, false, "Dump decoder output.");

ABSL_FLAG(bool, dump_encoder_input, false, "Dump encoder input.");

ABSL_FLAG(bool, dump_encoder_output, false, "Dump encoder output.");

namespace webrtc {
namespace test {

namespace {
using ::testing::Combine;
using ::testing::Values;
using VideoSourceSettings = VideoCodecTester::VideoSourceSettings;
using EncodingSettings = VideoCodecTester::EncodingSettings;
using FrameSettings = VideoCodecTester::FrameSettings;
using LayerId = VideoCodecTester::EncodingSettings::LayerId;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

const VideoCodecTester::VideoSourceSettings kFourPeople_1280x720_30 = {
    .name = "FourPeople_1280x720_30",
    .resolution = {.width = 1280, .height = 720},
    .framerate = Frequency::Hertz(30)};

std::unique_ptr<VideoEncoderFactory> CreateEncoderFactory(std::string impl) {
  std::unique_ptr<VideoEncoderFactory> factory;
  if (impl == "builtin") {
    return std::make_unique<InternalEncoderFactory>();
  }
#if defined(WEBRTC_ANDROID)
  InitializeAndroidObjects();
  return CreateAndroidEncoderFactory();
#endif
  return nullptr;
}

std::unique_ptr<VideoDecoderFactory> CreateDecoderFactory(std::string impl) {
  if (impl == "builtin") {
    return std::make_unique<InternalDecoderFactory>();
  }
#if defined(WEBRTC_ANDROID)
  InitializeAndroidObjects();
  return CreateAndroidDecoderFactory();
#endif
  return nullptr;
}

// TODO: remove. frame setting including target rate is known to tester.
void SetTargetRates(const FrameSettings& frame_settings,
                    std::vector<VideoCodecStats::Frame>& frames) {
  for (VideoCodecStats::Frame& frame : frames) {
    LayerId layer_id = {.spatial_idx = frame.spatial_idx,
                        .temporal_idx = frame.temporal_idx};
    const EncodingSettings& encoding_settings =
        frame_settings.at(frame.timestamp_rtp);
    const EncodingSettings::LayerSettings& layer_settings =
        encoding_settings.layer_settings.at(layer_id);
    frame.target_bitrate = layer_settings.bitrate;
    frame.target_framerate = layer_settings.framerate;
  }
}

std::string TestOutputPath() {
  std::string output_path =
      OutputPath() +
      ::testing::UnitTest::GetInstance()->current_test_info()->name();
  std::string output_dir = DirName(output_path);
  bool result = CreateDir(output_dir);
  RTC_CHECK(result) << "Cannot create " << output_dir;
  return output_path;
}
}  // namespace

std::unique_ptr<VideoCodecStats> RunEncodeDecodeTest(
    std::string codec_type,
    std::string codec_impl,
    const VideoSourceSettings& source_settings,
    const FrameSettings& frame_settings) {
  std::unique_ptr<VideoEncoderFactory> encoder_factory =
      CreateEncoderFactory(codec_impl);

  std::unique_ptr<VideoDecoderFactory> decoder_factory =
      CreateDecoderFactory(codec_impl);

  const EncodingSettings& encoding_settings = frame_settings.begin()->second;
  if (!decoder_factory
           ->QueryCodecSupport(encoding_settings.sdp_video_format,
                               /*reference_scaling=*/false)
           .is_supported) {
    decoder_factory = CreateDecoderFactory("builtin");
  }

  std::string output_path = TestOutputPath();

  VideoCodecTester::EncoderSettings encoder_settings;
  if (absl::GetFlag(FLAGS_dump_encoder_input)) {
    encoder_settings.encoder_input_base_path = output_path + "_enc_input";
  }
  if (absl::GetFlag(FLAGS_dump_encoder_output)) {
    encoder_settings.encoder_output_base_path = output_path + "_enc_output";
  }

  VideoCodecTester::DecoderSettings decoder_settings;
  if (absl::GetFlag(FLAGS_dump_decoder_input)) {
    decoder_settings.decoder_input_base_path = output_path + "_dec_input";
  }
  if (absl::GetFlag(FLAGS_dump_decoder_output)) {
    decoder_settings.decoder_output_base_path = output_path + "_dec_output";
  }

  std::unique_ptr<VideoCodecTester> tester = CreateVideoCodecTester();
  return tester->RunEncodeDecodeTest(source_settings, encoder_factory.get(),
                                     decoder_factory.get(), encoder_settings,
                                     decoder_settings, frame_settings);
}

#if 0
std::unique_ptr<VideoCodecStats> RunEncodeTest(
    std::string codec_type,
    std::string codec_impl,
    const VideoInfo& video_info,
    const std::map<int, EncodingSettings>& frame_settings,
    int num_frames) {
  std::unique_ptr<TestRawVideoSource> video_source =
      CreateVideoSource(video_info, frame_settings, num_frames);

  std::unique_ptr<TestEncoder> encoder =
      CreateEncoder(codec_type, codec_impl, frame_settings);
  if (encoder == nullptr) {
    return nullptr;
  }

  RTC_LOG(LS_INFO) << "Encoder implementation: "
                   << encoder->encoder()->GetEncoderInfo().implementation_name;

  VideoCodecTester::EncoderSettings encoder_settings;
  encoder_settings.pacing.mode =
      encoder->encoder()->GetEncoderInfo().is_hardware_accelerated
          ? PacingMode::kRealTime
          : PacingMode::kNoPacing;

  std::string output_path = TestOutputPath();
  if (save_codec_input) {
    encoder_settings.encoder_input_base_path = output_path + "_enc_input";
  }
  if (save_codec_output) {
    encoder_settings.encoder_output_base_path = output_path + "_enc_output";
  }

  std::unique_ptr<VideoCodecTester> tester = CreateVideoCodecTester();
  return tester->RunEncodeTest(video_source.get(), encoder.get(),
                               encoder_settings);
}
#endif

class SpatialQualityTest : public ::testing::TestWithParam<
                               std::tuple</*codec_type=*/std::string,
                                          /*codec_impl=*/std::string,
                                          VideoSourceSettings,
                                          std::tuple</*width=*/int,
                                                     /*height=*/int,
                                                     /*framerate_fps=*/double,
                                                     /*bitrate_kbps=*/int,
                                                     /*min_psnr=*/double>>> {
 public:
  static std::string TestParamsToString(
      const ::testing::TestParamInfo<SpatialQualityTest::ParamType>& info) {
    auto [codec_type, codec_impl, video_info, coding_settings] = info.param;
    auto [width, height, framerate_fps, bitrate_kbps, psnr] = coding_settings;
    return std::string(codec_type + codec_impl + video_info.name +
                       std::to_string(width) + "x" + std::to_string(height) +
                       "p" +
                       std::to_string(static_cast<int>(1000 * framerate_fps)) +
                       "mhz" + std::to_string(bitrate_kbps) + "kbps");
  }
};

TEST_P(SpatialQualityTest, SpatialQuality) {
  auto [codec_type, codec_impl, video_info, coding_settings] = GetParam();
  auto [width, height, framerate_fps, bitrate_kbps, psnr] = coding_settings;

  int duration_s = 10;
  int num_frames = duration_s * framerate_fps;

  FrameSettings frame_settings;
  uint32_t timestamp_rtp = 1;
  for (int frame_num = 0; frame_num < num_frames; ++frame_num) {
    frame_settings.emplace(
        timestamp_rtp,
        EncodingSettings{
            .sdp_video_format = SdpVideoFormat(codec_type),
            .scalability_mode = ScalabilityMode::kL2T1,
            .layer_settings = {
                {LayerId{.spatial_idx = 0, .temporal_idx = 0},
                 {.resolution = {.width = width, .height = height},
                  .framerate = Frequency::MilliHertz(1000 * framerate_fps),
                  .bitrate = DataRate::KilobitsPerSec(bitrate_kbps)}},
    
                {LayerId{.spatial_idx = 1, .temporal_idx = 0},
                 {.resolution = {.width = width / 2, .height = height / 2},
                  .framerate = Frequency::MilliHertz(1000 * framerate_fps),
                  .bitrate = DataRate::KilobitsPerSec(bitrate_kbps / 2)}}}});
    
    timestamp_rtp += k90kHz / Frequency::MilliHertz(1000 * framerate_fps);
  }

  std::unique_ptr<VideoCodecStats> stats =
      RunEncodeDecodeTest(codec_type, codec_impl, video_info, frame_settings);

  VideoCodecStats::Stream stream;
  if (stats != nullptr) {
    std::vector<VideoCodecStats::Frame> frames = stats->Slice();
    // TODO: pass frame settings to Aggregate.
    SetTargetRates(frame_settings, frames);
    stream = stats->Aggregate(frames);
    if (field_trial::IsEnabled("WebRTC-QuickPerfTest")) {
      EXPECT_GE(stream.psnr.y.GetAverage(), psnr);
    }
  }

  // TODO: all VideoCodecStats::LogMetrics() to dump per-frame stats. Only to
  // csv?
  stream.LogMetrics(
      GetGlobalMetricsLogger(),
      ::testing::UnitTest::GetInstance()->current_test_info()->name(),
      /*metadata=*/
      {{"codec_type", codec_type},
       {"codec_impl", codec_impl},
       {"video_name", video_info.name}});
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SpatialQualityTest,
    Combine(Values("AV1", "VP9", "VP8", "H264", "H265"),
#if defined(WEBRTC_ANDROID)
            Values("builtin", "mediacodec"),
#else
            Values("builtin"),
#endif
            Values(kFourPeople_1280x720_30),
            Values(std::make_tuple(320, 180, 30, 32, 28),
                   std::make_tuple(320, 180, 30, 64, 30),
                   std::make_tuple(320, 180, 30, 128, 33),
                   std::make_tuple(320, 180, 30, 256, 36),
                   std::make_tuple(640, 360, 30, 128, 31),
                   std::make_tuple(640, 360, 30, 256, 33),
                   std::make_tuple(640, 360, 30, 384, 35),
                   std::make_tuple(640, 360, 30, 512, 36),
                   std::make_tuple(1280, 720, 30, 256, 32),
                   std::make_tuple(1280, 720, 30, 512, 34),
                   std::make_tuple(1280, 720, 30, 1024, 37),
                   std::make_tuple(1280, 720, 30, 2048, 39))),
    SpatialQualityTest::TestParamsToString);

#if 0
class BitrateAdaptationTest
    : public ::testing::TestWithParam<
          std::tuple</*codec_type=*/std::string,
                     /*codec_impl=*/std::string,
                     VideoInfo,
                     std::pair</*bitrate_kbps=*/int, /*bitrate_kbps=*/int>>> {
 public:
  static std::string TestParamsToString(
      const ::testing::TestParamInfo<BitrateAdaptationTest::ParamType>& info) {
    auto [codec_type, codec_impl, video_info, bitrate_kbps] = info.param;
    return std::string(codec_type + codec_impl + video_info.name +
                       std::to_string(bitrate_kbps.first) + "kbps" +
                       std::to_string(bitrate_kbps.second) + "kbps");
  }
};


TEST_P(BitrateAdaptationTest, BitrateAdaptation) {
  auto [codec_type, codec_impl, video_info, bitrate_kbps] = GetParam();

  int duration_s = 10;  // Duration of fixed rate interval.
  int first_frame = duration_s * video_info.framerate.millihertz() / 1000;
  int num_frames = 2 * duration_s * video_info.framerate.millihertz() / 1000;

  std::map<int, EncodingSettings> frame_settings = {
      {0,
       {.layer_settings = {{LayerId{.spatial_idx = 0, .temporal_idx = 0},
                            {.resolution = {.width = 640, .height = 360},
                             .framerate = video_info.framerate,
                             .bitrate = DataRate::KilobitsPerSec(
                                 bitrate_kbps.first)}}}}},
      {first_frame,
       {.layer_settings = {
            {LayerId{.spatial_idx = 0, .temporal_idx = 0},
             {.resolution = {.width = 640, .height = 360},
              .framerate = video_info.framerate,
              .bitrate = DataRate::KilobitsPerSec(bitrate_kbps.second)}}}}}};

  std::unique_ptr<VideoCodecStats> stats = RunEncodeTest(
      codec_type, codec_impl, video_info, frame_settings, num_frames,
      /*save_codec_input=*/false, /*save_codec_output=*/false);

  VideoCodecStats::Stream stream;
  if (stats != nullptr) {
    std::vector<VideoCodecStats::Frame> frames =
        stats->Slice(VideoCodecStats::Filter{.first_frame = first_frame});
    SetTargetRates(frame_settings, frames);
    stream = stats->Aggregate(frames);
    if (field_trial::IsEnabled("WebRTC-QuickPerfTest")) {
      EXPECT_NEAR(stream.bitrate_mismatch_pct.GetAverage(), 0, 10);
      EXPECT_NEAR(stream.framerate_mismatch_pct.GetAverage(), 0, 10);
    }
  }

  stream.LogMetrics(
      GetGlobalMetricsLogger(),
      ::testing::UnitTest::GetInstance()->current_test_info()->name(),
      /*metadata=*/
      {{"codec_type", codec_type},
       {"codec_impl", codec_impl},
       {"video_name", video_info.name},
       {"rate_profile", std::to_string(bitrate_kbps.first) + "," +
                            std::to_string(bitrate_kbps.second)}});
}

INSTANTIATE_TEST_SUITE_P(All,
                         BitrateAdaptationTest,
                         Combine(Values("AV1", "VP9", "VP8", "H264", "H265"),
#if defined(WEBRTC_ANDROID)
                                 Values("builtin", "mediacodec"),
#else
                                 Values("builtin"),
#endif
                                 Values(kFourPeople_1280x720_30),
                                 Values(std::pair(1024, 512),
                                        std::pair(512, 1024))),
                         BitrateAdaptationTest::TestParamsToString);

class FramerateAdaptationTest
    : public ::testing::TestWithParam<std::tuple</*codec_type=*/std::string,
                                                 /*codec_impl=*/std::string,
                                                 VideoInfo,
                                                 std::pair<double, double>>> {
 public:
  static std::string TestParamsToString(
      const ::testing::TestParamInfo<FramerateAdaptationTest::ParamType>&
          info) {
    auto [codec_type, codec_impl, video_info, framerate_fps] = info.param;
    return std::string(
        codec_type + codec_impl + video_info.name +
        std::to_string(static_cast<int>(1000 * framerate_fps.first)) + "mhz" +
        std::to_string(static_cast<int>(1000 * framerate_fps.second)) + "mhz");
  }
};

TEST_P(FramerateAdaptationTest, FramerateAdaptation) {
  auto [codec_type, codec_impl, video_info, framerate_fps] = GetParam();

  int duration_s = 10;  // Duration of fixed rate interval.
  int first_frame = static_cast<int>(duration_s * framerate_fps.first);
  int num_frames = static_cast<int>(
      duration_s * (framerate_fps.first + framerate_fps.second));

  std::map<int, EncodingSettings> frame_settings = {
      {0,
       {.layer_settings = {{LayerId{.spatial_idx = 0, .temporal_idx = 0},
                            {.resolution = {.width = 640, .height = 360},
                             .framerate = Frequency::MilliHertz(
                                 1000 * framerate_fps.first),
                             .bitrate = DataRate::KilobitsPerSec(512)}}}}},
      {first_frame,
       {.layer_settings = {
            {LayerId{.spatial_idx = 0, .temporal_idx = 0},
             {.resolution = {.width = 640, .height = 360},
              .framerate = Frequency::MilliHertz(1000 * framerate_fps.second),
              .bitrate = DataRate::KilobitsPerSec(512)}}}}}};

  std::unique_ptr<VideoCodecStats> stats = RunEncodeTest(
      codec_type, codec_impl, video_info, frame_settings, num_frames,
      /*save_codec_input=*/false, /*save_codec_output=*/false);

  VideoCodecStats::Stream stream;
  if (stats != nullptr) {
    std::vector<VideoCodecStats::Frame> frames =
        stats->Slice(VideoCodecStats::Filter{.first_frame = first_frame});
    SetTargetRates(frame_settings, frames);
    stream = stats->Aggregate(frames);
    if (field_trial::IsEnabled("WebRTC-QuickPerfTest")) {
      EXPECT_NEAR(stream.bitrate_mismatch_pct.GetAverage(), 0, 10);
      EXPECT_NEAR(stream.framerate_mismatch_pct.GetAverage(), 0, 10);
    }
  }

  stream.LogMetrics(
      GetGlobalMetricsLogger(),
      ::testing::UnitTest::GetInstance()->current_test_info()->name(),
      /*metadata=*/
      {{"codec_type", codec_type},
       {"codec_impl", codec_impl},
       {"video_name", video_info.name},
       {"rate_profile", std::to_string(framerate_fps.first) + "," +
                            std::to_string(framerate_fps.second)}});
}

INSTANTIATE_TEST_SUITE_P(All,
                         FramerateAdaptationTest,
                         Combine(Values("AV1", "VP9", "VP8", "H264", "H265"),
#if defined(WEBRTC_ANDROID)
                                 Values("builtin", "mediacodec"),
#else
                                 Values("builtin"),
#endif
                                 Values(kFourPeople_1280x720_30),
                                 Values(std::pair(30, 15), std::pair(15, 30))),
                         FramerateAdaptationTest::TestParamsToString);
#endif
}  // namespace test

}  // namespace webrtc
