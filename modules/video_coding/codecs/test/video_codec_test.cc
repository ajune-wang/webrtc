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
#include <numeric>
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
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#if defined(WEBRTC_ANDROID)
#include "modules/video_coding/codecs/test/android_codec_factory_helper.h"
#endif
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video/video_bitrate_allocator.h"
#include "modules/video_coding/codecs/av1/av1_svc_config.h"
#include "modules/video_coding/codecs/test/video_codec_analyzer.h"
#include "modules/video_coding/codecs/vp9/svc_config.h"
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
using LayerSettings = VideoCodecTester::EncodingSettings::LayerSettings;
using FrameSettings = VideoCodecTester::FrameSettings;
using LayerId = VideoCodecTester::EncodingSettings::LayerId;
using R = std::vector<std::vector<int>>;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

struct RawVideoInfo {
  std::string name;
  Resolution resolution;
  Frequency framerate;
};

const RawVideoInfo kFourPeople_1280x720_30 = {
    .name = "FourPeople_1280x720_30",
    .resolution = {.width = 1280, .height = 720},
    .framerate = Frequency::Hertz(30)};

const RawVideoInfo kVidyo1_1280x720_30 = {
    .name = "vidyo1_1280x720_30",
    .resolution = {.width = 1280, .height = 720},
    .framerate = Frequency::Hertz(30)};

const RawVideoInfo kVidyo4_1280x720_30 = {
    .name = "vidyo4_1280x720_30",
    .resolution = {.width = 1280, .height = 720},
    .framerate = Frequency::Hertz(30)};

std::unique_ptr<VideoEncoderFactory> CreateEncoderFactory(std::string impl) {
  std::unique_ptr<VideoEncoderFactory> factory;
  if (impl == "builtin") {
    return CreateBuiltinVideoEncoderFactory();
  }
#if defined(WEBRTC_ANDROID)
  InitializeAndroidObjects();
  return CreateAndroidEncoderFactory();
#endif
  return nullptr;
}

std::unique_ptr<VideoDecoderFactory> CreateDecoderFactory(std::string impl) {
  if (impl == "builtin") {
    return CreateBuiltinVideoDecoderFactory();
  }
#if defined(WEBRTC_ANDROID)
  InitializeAndroidObjects();
  return CreateAndroidDecoderFactory();
#endif
  return nullptr;
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
    const RawVideoInfo& video_info,
    const FrameSettings& frame_settings) {
  VideoSourceSettings source_settings{
      .file_path = ResourcePath(video_info.name, "yuv"),
      .resolution = video_info.resolution,
      .framerate = video_info.framerate};

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

// TODO: move to tester
// TODO: move to webrtc/test/ alltogether with tester
std::map<LayerId, LayerSettings> ConfigureLayers(
    VideoCodecType codec_type,
    ScalabilityMode scalability_mode,
    int width,
    int height,
    int bitrate_kbps,
    double framerate_fps) {
  VideoCodec vc;
  vc.codecType = codec_type;
  vc.width = width;
  vc.height = height;
  vc.startBitrate = bitrate_kbps;
  vc.maxBitrate = bitrate_kbps;
  vc.minBitrate = 0;
  vc.maxFramerate = static_cast<uint32_t>(framerate_fps);
  vc.active = true;
  vc.qpMax = 63;
  vc.numberOfSimulcastStreams = 0;
  vc.mode = webrtc::VideoCodecMode::kRealtimeVideo;
  vc.SetFrameDropEnabled(true);
  vc.SetScalabilityMode(scalability_mode);

  int num_spatial_layers = ScalabilityModeToNumSpatialLayers(scalability_mode);
  int num_temporal_layers =
      ScalabilityModeToNumTemporalLayers(scalability_mode);

  if (vc.codecType == kVideoCodecAV1) {
    bool result = SetAv1SvcConfig(vc, num_spatial_layers, num_temporal_layers);
    RTC_CHECK(result) << "SetAv1SvcConfig failed";
  } else if (vc.codecType == kVideoCodecVP9) {
    *(vc.VP9()) = VideoEncoder::GetDefaultVp9Settings();
    vc.VP9()->SetNumberOfTemporalLayers(num_temporal_layers);

    const std::vector<SpatialLayer> spatialLayers = GetVp9SvcConfig(vc);
    for (size_t i = 0; i < spatialLayers.size(); ++i) {
      vc.spatialLayers[i] = spatialLayers[i];
    }
  } else if (vc.codecType == kVideoCodecVP8) {
    *(vc.VP8()) = VideoEncoder::GetDefaultVp8Settings();
    vc.VP8()->SetNumberOfTemporalLayers(num_temporal_layers);
    // TODO: set up S>1 modes.
    vc.simulcastStream[0].width = vc.width;
    vc.simulcastStream[0].height = vc.height;
  } else if (vc.codecType == kVideoCodecH264) {
    *(vc.H264()) = VideoEncoder::GetDefaultH264Settings();
  }

  if (*vc.GetScalabilityMode() != scalability_mode) {
    RTC_LOG(LS_INFO) << "Scalability mode has been changed from "
                     << ScalabilityModeToString(scalability_mode) << " to "
                     << ScalabilityModeToString(*vc.GetScalabilityMode());
  }

  num_spatial_layers =
      ScalabilityModeToNumSpatialLayers(*vc.GetScalabilityMode());
  num_temporal_layers =
      ScalabilityModeToNumTemporalLayers(*vc.GetScalabilityMode());

  std::unique_ptr<VideoBitrateAllocator> bitrate_allocator =
      CreateBuiltinVideoBitrateAllocatorFactory()->CreateVideoBitrateAllocator(
          vc);

  VideoBitrateAllocation bitrate_allocation = bitrate_allocator->Allocate(
      VideoBitrateAllocationParameters(1000 * bitrate_kbps, framerate_fps));

  std::map<LayerId, LayerSettings> layer_settings;
  for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
    int width = codec_type == kVideoCodecVP8 ? vc.simulcastStream[sidx].width
                                             : vc.spatialLayers[sidx].width;
    int height = codec_type == kVideoCodecVP8 ? vc.simulcastStream[sidx].height
                                              : vc.spatialLayers[sidx].height;
    for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
      layer_settings.emplace(
          LayerId{.spatial_idx = sidx, .temporal_idx = tidx},
          LayerSettings{.resolution = {.width = width, .height = height},
                        .framerate = Frequency::MilliHertz(
                            static_cast<int>(1000 * framerate_fps) >>
                            (num_temporal_layers - tidx - 1)),
                        .bitrate = DataRate::BitsPerSec(
                            bitrate_allocation.GetBitrate(sidx, tidx))});
    }
  }

  return layer_settings;
}

class SpatialQualityTest : public ::testing::TestWithParam<std::tuple<
                               /*codec_type=*/std::string,
                               /*codec_impl=*/std::string,
                               RawVideoInfo,
                               std::tuple</*width=*/int,
                                          /*height=*/int,
                                          /*framerate_fps=*/double,
                                          /*scalability_mode=*/std::string,
                                          /*bitrate_kbps=*/R>>> {
 public:
  static std::string TestParamsToString(
      const ::testing::TestParamInfo<SpatialQualityTest::ParamType>& info) {
    auto [codec_type, codec_impl, video_info, coding_settings] = info.param;
    auto [width, height, framerate_fps, scalability_mode, bitrate] =
        coding_settings;

    int total_bitrate_kbps = 0;
    for (auto layer_bitrates : bitrate) {
      total_bitrate_kbps +=
          std::accumulate(layer_bitrates.begin(), layer_bitrates.end(), 0);
    }
    return std::string(codec_type + codec_impl + video_info.name +
                       std::to_string(width) + "x" + std::to_string(height) +
                       "p" + scalability_mode +
                       std::to_string(static_cast<int>(1000 * framerate_fps)) +
                       "mhz" + std::to_string(total_bitrate_kbps) + "kbps");
  }
};

TEST_P(SpatialQualityTest, SpatialQuality) {
  auto [codec_name, codec_impl, video_info, coding_settings] = GetParam();
  auto [width, height, framerate_fps, scalability_name, bitrate_kbps] =
      coding_settings;
  int duration_s = 3;  //@@@ 10;
  int num_frames = duration_s * framerate_fps;

  VideoCodecType codec_type = PayloadStringToCodecType(codec_name);
  ScalabilityMode scalability_mode =
      *ScalabilityModeFromString(scalability_name);
  int num_spatial_layers = ScalabilityModeToNumSpatialLayers(scalability_mode);
  int num_temporal_layers =
      ScalabilityModeToNumTemporalLayers(scalability_mode);

  FrameSettings frame_settings;
  uint32_t timestamp_rtp = 1;
  for (int frame_num = 0; frame_num < num_frames; ++frame_num) {
    std::map<LayerId, LayerSettings> layer_settings;
    if ((num_spatial_layers > 1 || num_temporal_layers > 1) &&
        bitrate_kbps.size() == 1u && bitrate_kbps[0].size() == 1u) {
      // Only total bitrate is configured. Distibute the bitrate across layers
      // using a standard rate allocator.
      layer_settings =
          ConfigureLayers(codec_type, scalability_mode, width, height,
                          bitrate_kbps[0][0], framerate_fps);
    } else {
      for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
        int layer_width = width >> (num_spatial_layers - sidx - 1);
        int layer_height = height >> (num_spatial_layers - sidx - 1);
        for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
          layer_settings.emplace(
              LayerId{.spatial_idx = int(sidx), .temporal_idx = int(tidx)},
              LayerSettings{
                  .resolution = {.width = layer_width, .height = layer_height},
                  .framerate = Frequency::MilliHertz(
                      static_cast<int>(1000 * framerate_fps) >>
                      (num_temporal_layers - tidx - 1)),
                  .bitrate =
                      DataRate::KilobitsPerSec(bitrate_kbps[sidx][tidx])});
        }
      }
    }

    frame_settings.emplace(
        timestamp_rtp,
        EncodingSettings{.sdp_video_format = SdpVideoFormat(codec_name),
                         .scalability_mode = scalability_mode,
                         .layer_settings = layer_settings});
    timestamp_rtp += k90kHz / Frequency::MilliHertz(1000 * framerate_fps);
  }

  std::unique_ptr<VideoCodecStats> stats =
      RunEncodeDecodeTest(codec_name, codec_impl, video_info, frame_settings);

#if 0
  for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
    for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
      VideoCodecStats::Stream stream;
      if (stats != nullptr) {
        stream = stats->Aggregate(
            VideoCodecStats::Filter{.spatial_idx = sidx, .temporal_idx = tidx});
      }

      std::string metric_name_prefix =
          "sl" + std::to_string(sidx) + "tl" + std::to_string(tidx) + "_";
      stream.LogMetrics(
          GetGlobalMetricsLogger(),
          ::testing::UnitTest::GetInstance()->current_test_info()->name(),
          metric_name_prefix,
          /*metadata=*/
          {{"codec_type", codec_name},
           {"codec_impl", codec_impl},
           {"video_name", video_info.name}});
    }
  }
#endif

  if (num_spatial_layers > 0 || num_temporal_layers > 0) {
    VideoCodecStats::Stream stream;
    if (stats != nullptr) {
      stream = stats->Aggregate();
    }

    // TODO: all VideoCodecStats::LogMetrics() to dump per-frame stats. Only to
    // csv?
    stream.LogMetrics(
        GetGlobalMetricsLogger(),
        ::testing::UnitTest::GetInstance()->current_test_info()->name(),
        /*metric_name_prefix=*/"",
        /*metadata=*/
        {{"codec_type", codec_name},
         {"codec_impl", codec_impl},
         {"video_name", video_info.name}});
  }
}

INSTANTIATE_TEST_SUITE_P(
    Singlecast,
    SpatialQualityTest,
    Combine(Values("AV1", "VP9", "VP8", "H264", "H265"),
#if defined(WEBRTC_ANDROID)
            Values("builtin", "mediacodec"),
#else
            Values("builtin"),
#endif
            Values(kFourPeople_1280x720_30),
            Values(std::make_tuple(320, 180, 30, "L1T1", R{{32}}),
                   std::make_tuple(320, 180, 30, "L1T1", R{{64}}),
                   std::make_tuple(320, 180, 30, "L1T1", R{{128}}),
                   std::make_tuple(320, 180, 30, "L1T1", R{{256}}),
                   std::make_tuple(640, 360, 30, "L1T1", R{{128}}),
                   std::make_tuple(640, 360, 30, "L1T1", R{{256}}),
                   std::make_tuple(640, 360, 30, "L1T1", R{{384}}),
                   std::make_tuple(640, 360, 30, "L1T1", R{{512}}),
                   std::make_tuple(1280, 720, 30, "L1T1", R{{256}}),
                   std::make_tuple(1280, 720, 30, "L1T1", R{{512}}),
                   std::make_tuple(1280, 720, 30, "L1T1", R{{1024}}),
                   std::make_tuple(1280, 720, 30, "L1T1", R{{2048}}))),
    SpatialQualityTest::TestParamsToString);

INSTANTIATE_TEST_SUITE_P(
    Svc,
    SpatialQualityTest,
    Combine(Values("AV1", "VP9", "VP8"),
#if defined(WEBRTC_ANDROID)
            Values("builtin", "mediacodec"),
#else
            Values("builtin"),
#endif
            Values(kFourPeople_1280x720_30,
                   kVidyo1_1280x720_30,
                   kVidyo4_1280x720_30),
            Values(std::make_tuple(320, 180, 30, "L1T3", R{{200}}),
                   std::make_tuple(320, 180, 30, "L1T3", R{{142}}),
                   std::make_tuple(320, 180, 30, "L1T3", R{{150}}),
                   std::make_tuple(320, 180, 30, "L1T3", R{{256}}),
                   std::make_tuple(320, 180, 30, "L1T3", R{{300}}))),
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
