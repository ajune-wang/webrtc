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
#include "test/test_flags.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

ABSL_FLAG(std::string, input_video, "FourPeople_1280x720_30", "Input video.");

ABSL_FLAG(std::string, codec_type, "AV1", "Codec type.");

ABSL_FLAG(std::string, codec_impl, "builtin", "Codec implementation.");

ABSL_FLAG(std::string, scalability_mode, "L1T1", "Scalability mode.");

ABSL_FLAG(int, width, 1280, "Width.");

ABSL_FLAG(int, height, 720, "Height.");

ABSL_FLAG(std::vector<std::string>,
          bitrate_kbps,
          std::vector<std::string>({"1024"}),
          "Bitrate, kbps.");

ABSL_FLAG(double, framerate_fps, 30.0, "Framerate, fps.");

ABSL_FLAG(int, num_frames, 300, "Number of frames to encode/decode.");

ABSL_FLAG(bool, dump_decoder_input, false, "Dump decoder input.");

ABSL_FLAG(bool, dump_decoder_output, false, "Dump decoder output.");

ABSL_FLAG(bool, dump_encoder_input, false, "Dump encoder input.");

ABSL_FLAG(bool, dump_encoder_output, false, "Dump encoder output.");

ABSL_FLAG(bool, write_csv, false, "Write metrics to a CSV file.");

ABSL_FLAG(std::string, test_name, "", "Test name.");

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
  std::string file_name;
  Resolution resolution;
  Frequency framerate;
};

const std::map<std::string, RawVideoInfo> kSourceVideos = {
    {"FourPeople_1280x720_30",
     {.file_name = "FourPeople_1280x720_30",
      .resolution = {.width = 1280, .height = 720},
      .framerate = Frequency::Hertz(30)}},
    {"vidyo1_1280x720_30",
     {.file_name = "vidyo1_1280x720_30",
      .resolution = {.width = 1280, .height = 720},
      .framerate = Frequency::Hertz(30)}},
    {"vidyo4_1280x720_30",
     {.file_name = "vidyo4_1280x720_30",
      .resolution = {.width = 1280, .height = 720},
      .framerate = Frequency::Hertz(30)}},
    {"KristenAndSara_1280x720_30",
     {.file_name = "KristenAndSara_1280x720_30",
      .resolution = {.width = 1280, .height = 720},
      .framerate = Frequency::Hertz(30)}},
    {"Johnny_1280x720_30",
     {.file_name = "Johnny_1280x720_30",
      .resolution = {.width = 1280, .height = 720},
      .framerate = Frequency::Hertz(30)}}};

template <typename Range>
std::string StrJoin(const Range& seq, absl::string_view delimiter) {
  rtc::StringBuilder sb;
  int idx = 0;

  for (const typename Range::value_type& elem : seq) {
    if (idx > 0) {
      sb << delimiter;
    }
    sb << elem;

    ++idx;
  }
  return sb.Release();
}

std::string TestParamsToString(std::string video_name,
                               std::string codec_type,
                               std::string codec_impl,
                               std::string scalability_mode,
                               int width,
                               int height,
                               const std::vector<double>& framerate_fps,
                               const std::vector<int>& bitrate_kbps,
                               std::string separator = "") {
  std::vector<double> rounded_framerate_fps;
  std::transform(
      framerate_fps.begin(), framerate_fps.end(),
      std::back_inserter(rounded_framerate_fps),
      [](const double& fps) { return static_cast<int>(100 * fps) / 100.0; });

  std::string name = video_name;
  name += separator + codec_type;
  name += separator + codec_impl;
  name += separator + scalability_mode;
  name += separator + std::to_string(width) + "x" + std::to_string(height);
  name += separator + StrJoin(rounded_framerate_fps, "_") + "fps";
  name += separator + StrJoin(bitrate_kbps, "_") + "kbps";
  return name;
}

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

std::string TestName(absl::optional<std::string> test_name = absl::nullopt) {
  return test_name.value_or(
      ::testing::UnitTest::GetInstance()->current_test_info()->name());
}

std::string TestOutputPath(std::string test_name) {
  std::string output_path = OutputPath() + test_name;
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
    const FrameSettings& frame_settings,
    std::string test_name) {
  VideoSourceSettings source_settings{
      .file_path = ResourcePath(video_info.file_name, "yuv"),
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

  std::string output_path = TestOutputPath(test_name);

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

std::unique_ptr<VideoCodecStats> RunEncodeTest(
    std::string codec_type,
    std::string codec_impl,
    const RawVideoInfo& video_info,
    const FrameSettings& frame_settings) {
  VideoSourceSettings source_settings{
      .file_path = ResourcePath(video_info.file_name, "yuv"),
      .resolution = video_info.resolution,
      .framerate = video_info.framerate};

  std::unique_ptr<VideoEncoderFactory> encoder_factory =
      CreateEncoderFactory(codec_impl);

  std::string output_path = TestOutputPath(TestName());
  VideoCodecTester::EncoderSettings encoder_settings;
  if (absl::GetFlag(FLAGS_dump_encoder_input)) {
    encoder_settings.encoder_input_base_path = output_path + "_enc_input";
  }
  if (absl::GetFlag(FLAGS_dump_encoder_output)) {
    encoder_settings.encoder_output_base_path = output_path + "_enc_output";
  }

  std::unique_ptr<VideoCodecTester> tester = CreateVideoCodecTester();
  return tester->RunEncodeTest(source_settings, encoder_factory.get(),
                               encoder_settings, frame_settings);
}

// TODO: move to tester?
std::tuple<std::vector<DataRate>, ScalabilityMode>
SplitBitrateAndUpdateScalabilityMode(std::string codec_type,
                                     ScalabilityMode scalability_mode,
                                     int width,
                                     int height,
                                     std::vector<int> layer_bitrates_kbps,
                                     double framerate_fps) {
  int num_spatial_layers = ScalabilityModeToNumSpatialLayers(scalability_mode);
  int num_temporal_layers =
      ScalabilityModeToNumTemporalLayers(scalability_mode);

  if (layer_bitrates_kbps.size() == 1 &&
      (num_spatial_layers > 1 || num_temporal_layers > 1)) {
    VideoCodec vc;
    vc.codecType = PayloadStringToCodecType(codec_type);
    vc.width = width;
    vc.height = height;
    vc.startBitrate = layer_bitrates_kbps[0];
    vc.maxBitrate = layer_bitrates_kbps[0];
    vc.minBitrate = 0;
    vc.maxFramerate = static_cast<uint32_t>(framerate_fps);
    vc.active = true;
    vc.qpMax = 63;
    vc.numberOfSimulcastStreams = 0;
    vc.mode = webrtc::VideoCodecMode::kRealtimeVideo;
    vc.SetFrameDropEnabled(true);
    vc.SetScalabilityMode(scalability_mode);

    int num_spatial_layers =
        ScalabilityModeToNumSpatialLayers(scalability_mode);
    int num_temporal_layers =
        ScalabilityModeToNumTemporalLayers(scalability_mode);

    if (vc.codecType == kVideoCodecAV1) {
      bool result =
          SetAv1SvcConfig(vc, num_spatial_layers, num_temporal_layers);
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
      RTC_LOG(LS_WARNING) << "Scalability mode changed from "
                          << ScalabilityModeToString(scalability_mode) << " to "
                          << ScalabilityModeToString(*vc.GetScalabilityMode());
    }

    num_spatial_layers =
        ScalabilityModeToNumSpatialLayers(*vc.GetScalabilityMode());
    num_temporal_layers =
        ScalabilityModeToNumTemporalLayers(*vc.GetScalabilityMode());

    std::unique_ptr<VideoBitrateAllocator> bitrate_allocator =
        CreateBuiltinVideoBitrateAllocatorFactory()
            ->CreateVideoBitrateAllocator(vc);

    VideoBitrateAllocation bitrate_allocation =
        bitrate_allocator->Allocate(VideoBitrateAllocationParameters(
            1000 * layer_bitrates_kbps[0], framerate_fps));

    std::vector<DataRate> bitrate;
    for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
      for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
        bitrate.push_back(
            DataRate::BitsPerSec(bitrate_allocation.GetBitrate(sidx, tidx)));
      }
    }

    return std::make_tuple(bitrate, *vc.GetScalabilityMode());
  }

  RTC_CHECK(static_cast<int>(layer_bitrates_kbps.size()) ==
            num_spatial_layers * num_temporal_layers)
      << "When configured explicitly, the bitrates must be provided for all "
         "spatial and temporal layers.";

  std::vector<DataRate> layer_bitrates;
  std::transform(layer_bitrates_kbps.begin(), layer_bitrates_kbps.end(),
                 std::back_inserter(layer_bitrates),
                 [](const auto& bitrate_kbps) {
                   return DataRate::KilobitsPerSec(bitrate_kbps);
                 });
  return std::make_tuple(layer_bitrates, scalability_mode);
}

// TODO: move to tester?
FrameSettings CreateFrameSettings(std::string codec_type,
                                  std::string scalability_name,
                                  int width,
                                  int height,
                                  std::vector<int> bitrate_kbps,
                                  double framerate_fps,
                                  int num_frames,
                                  uint32_t initial_timestamp_rtp = 90000) {
  auto [layer_bitrate, scalability_mode] = SplitBitrateAndUpdateScalabilityMode(
      codec_type, *ScalabilityModeFromString(scalability_name), width, height,
      bitrate_kbps, framerate_fps);

  int num_spatial_layers = ScalabilityModeToNumSpatialLayers(scalability_mode);
  int num_temporal_layers =
      ScalabilityModeToNumTemporalLayers(scalability_mode);

  std::map<LayerId, LayerSettings> layer_settings;
  for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
    int layer_width = width >> (num_spatial_layers - sidx - 1);
    int layer_height = height >> (num_spatial_layers - sidx - 1);
    for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
      double layer_framerate_fps =
          framerate_fps / (1 << (num_temporal_layers - tidx - 1));
      layer_settings.emplace(
          LayerId{.spatial_idx = sidx, .temporal_idx = tidx},
          LayerSettings{
              .resolution = {.width = layer_width, .height = layer_height},
              .framerate = Frequency::MilliHertz(
                  static_cast<int>(1000 * layer_framerate_fps)),
              .bitrate = layer_bitrate[sidx * num_temporal_layers + tidx]});
    }
  }

  FrameSettings frame_settings;
  uint32_t timestamp_rtp = initial_timestamp_rtp;
  for (int frame_num = 0; frame_num < num_frames; ++frame_num) {
    frame_settings.emplace(
        timestamp_rtp,
        EncodingSettings{.sdp_video_format = SdpVideoFormat(codec_type),
                         .scalability_mode = scalability_mode,
                         .layer_settings = layer_settings});

    timestamp_rtp += k90kHz / Frequency::MilliHertz(1000 * framerate_fps);
  }

  return frame_settings;
}

std::unique_ptr<VideoCodecStats> TestEncodeDecode(RawVideoInfo video_info,
                                                  std::string codec_type,
                                                  std::string codec_impl,
                                                  std::string scalability_mode,
                                                  int width,
                                                  int height,
                                                  double framerate_fps,
                                                  std::vector<int> bitrate_kbps,
                                                  int num_frames,
                                                  std::string test_name) {
  FrameSettings frame_settings =
      CreateFrameSettings(codec_type, scalability_mode, width, height,
                          bitrate_kbps, framerate_fps, num_frames);

  std::unique_ptr<VideoCodecStats> stats = RunEncodeDecodeTest(
      codec_type, codec_impl, video_info, frame_settings, test_name);

  VideoCodecStats::Stream stream;
  if (stats != nullptr) {
    stream = stats->Aggregate();
  }

  stream.LogMetrics(GetGlobalMetricsLogger(), test_name,
                    /*metric_name_prefix=*/"",
                    /*metadata=*/
                    {{"video_name", video_info.file_name},
                     {"codec_type", codec_type},
                     {"codec_impl", codec_impl},
                     {"scalability_mode", scalability_mode}});

  int num_spatial_layers = ScalabilityModeToNumSpatialLayers(
      *ScalabilityModeFromString(scalability_mode));
  int num_temporal_layers = ScalabilityModeToNumTemporalLayers(
      *ScalabilityModeFromString(scalability_mode));

  for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
    for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
      VideoCodecStats::Stream stream;
      if (stats != nullptr) {
        stream = stats->Aggregate(
            {.layer_id = {{.spatial_idx = sidx, .temporal_idx = tidx}}});
      }

      // TODO: All metrics will be prefixed. This is Ok for reporting and dash
      // boards. But this is not convenient for external analysis where we can
      // read layer id from metadata.
      std::string prefix =
          "s" + std::to_string(sidx) + "t" + std::to_string(tidx) + "_";
      stream.LogMetrics(GetGlobalMetricsLogger(), test_name, prefix,
                        /*metadata=*/
                        {{"codec_type", codec_type},
                         {"codec_impl", codec_impl},
                         {"video_name", video_info.file_name},
                         {"scalability_mode", scalability_mode}});
    }
  }

  if (absl::GetFlag(FLAGS_write_csv)) {
    std::string csv_file_path = TestOutputPath(test_name) + ".csv";
    stats->LogMetrics(
        csv_file_path, stats->Slice(),
        /*metadata=*/
        {{"codec_type", absl::GetFlag(FLAGS_codec_type)},
         {"codec_impl", absl::GetFlag(FLAGS_codec_impl)},
         {"video_name", video_info.file_name},
         {"scalability_mode", absl::GetFlag(FLAGS_scalability_mode)},
         {"test_name", test_name}});
  }

  return stats;
}

TEST(VideoCodecTest, EncodeDecode) {
  std::vector<std::string> bitrate_str = absl::GetFlag(FLAGS_bitrate_kbps);
  std::vector<int> bitrate_kbps;
  std::transform(bitrate_str.begin(), bitrate_str.end(),
                 std::back_inserter(bitrate_kbps),
                 [](const std::string& str) { return std::stoi(str); });

  std::string test_name = absl::GetFlag(FLAGS_test_name);
  if (test_name.empty()) {
    test_name = TestParamsToString(
        absl::GetFlag(FLAGS_input_video), absl::GetFlag(FLAGS_codec_type),
        absl::GetFlag(FLAGS_codec_impl), absl::GetFlag(FLAGS_scalability_mode),
        absl::GetFlag(FLAGS_width), absl::GetFlag(FLAGS_height),
        {absl::GetFlag(FLAGS_framerate_fps)}, bitrate_kbps, /*separator=*/"-");
  }

  std::unique_ptr<VideoCodecStats> stats = TestEncodeDecode(
      kSourceVideos.at(absl::GetFlag(FLAGS_input_video)),
      absl::GetFlag(FLAGS_codec_type), absl::GetFlag(FLAGS_codec_impl),
      absl::GetFlag(FLAGS_scalability_mode), absl::GetFlag(FLAGS_width),
      absl::GetFlag(FLAGS_height), absl::GetFlag(FLAGS_framerate_fps),
      bitrate_kbps, absl::GetFlag(FLAGS_num_frames), test_name);
}

class SpatialQualityTest : public ::testing::TestWithParam<std::tuple<
                               /*codec_type=*/std::string,
                               /*codec_impl=*/std::string,
                               RawVideoInfo,
                               std::tuple</*width=*/int,
                                          /*height=*/int,
                                          /*framerate_fps=*/double,
                                          /*scalability_mode=*/std::string,
                                          /*bitrate_kbps=*/int>>> {
 public:
  static std::string ToString(
      const ::testing::TestParamInfo<SpatialQualityTest::ParamType>& info) {
    auto [codec_type, codec_impl, video_info, coding_settings] = info.param;
    auto [width, height, framerate_fps, scalability_mode, bitrate_kbps] =
        coding_settings;
    return TestParamsToString(video_info.file_name, codec_type, codec_impl,
                              scalability_mode, width, height, {framerate_fps},
                              {bitrate_kbps});
  }
};

TEST_P(SpatialQualityTest, SpatialQuality) {
  auto [codec_type, codec_impl, video_info, coding_settings] = GetParam();
  auto [width, height, framerate_fps, scalability_mode, bitrate_kbps] =
      coding_settings;
  int duration_s = 10;
  int num_frames = duration_s * framerate_fps;
  TestEncodeDecode(
      video_info, codec_type, codec_impl, scalability_mode, width, height,
      framerate_fps, {bitrate_kbps}, num_frames,
      ::testing::UnitTest::GetInstance()->current_test_info()->name());
}

// TODO: no need R. total bitrate should be enough for this test.
INSTANTIATE_TEST_SUITE_P(
    Singlecast,
    SpatialQualityTest,
    Combine(Values("AV1", "VP9", "VP8", "H264", "H265"),
#if defined(WEBRTC_ANDROID)
            Values("builtin", "mediacodec"),
#else
            Values("builtin"),
#endif
            Values(kSourceVideos.at("FourPeople_1280x720_30")),
            Values(std::make_tuple(320, 180, 30, "L1T1", 32),
                   std::make_tuple(320, 180, 30, "L1T1", 64),
                   std::make_tuple(320, 180, 30, "L1T1", 128),
                   std::make_tuple(320, 180, 30, "L1T1", 256),
                   std::make_tuple(640, 360, 30, "L1T1", 128),
                   std::make_tuple(640, 360, 30, "L1T1", 256),
                   std::make_tuple(640, 360, 30, "L1T1", 384),
                   std::make_tuple(640, 360, 30, "L1T1", 512),
                   std::make_tuple(1280, 720, 30, "L1T1", 256),
                   std::make_tuple(1280, 720, 30, "L1T1", 512),
                   std::make_tuple(1280, 720, 30, "L1T1", 1024),
                   std::make_tuple(1280, 720, 30, "L1T1", 2048))),
    SpatialQualityTest::ToString);

class BitrateAdaptationTest
    : public ::testing::TestWithParam<
          std::tuple</*codec_type=*/std::string,
                     /*codec_impl=*/std::string,
                     RawVideoInfo,
                     std::pair</*bitrate_kbps=*/int, /*bitrate_kbps=*/int>>> {
 public:
  static std::string ToString(
      const ::testing::TestParamInfo<BitrateAdaptationTest::ParamType>& info) {
    auto [codec_type, codec_impl, video_info, bitrate_kbps] = info.param;
    return std::string(codec_type + codec_impl + video_info.file_name +
                       std::to_string(bitrate_kbps.first) + "kbps" +
                       std::to_string(bitrate_kbps.second) + "kbps");
  }
};

TEST_P(BitrateAdaptationTest, BitrateAdaptation) {
  auto [codec_type, codec_impl, video_info, bitrate_kbps] = GetParam();

  int duration_s = 10;  // Duration of fixed rate interval.
  int num_frames = 2 * duration_s * video_info.framerate.millihertz() / 1000;

  FrameSettings frame_settings = CreateFrameSettings(
      codec_type, /*scalability_mode=*/"L1T1", /*width=*/640, /*height=*/360,
      {bitrate_kbps.first}, 30.0, num_frames / 2);

  uint32_t initial_timestamp_rtp =
      frame_settings.rbegin()->first + k90kHz / Frequency::Hertz(30);
  FrameSettings frame_settings2 = CreateFrameSettings(
      codec_type, /*scalability_mode=*/"L1T1", /*width=*/640, /*height=*/360,
      {bitrate_kbps.first}, 30.0, num_frames / 2, initial_timestamp_rtp);

  frame_settings.merge(frame_settings2);

  std::unique_ptr<VideoCodecStats> stats =
      RunEncodeTest(codec_type, codec_impl, video_info, frame_settings);

  VideoCodecStats::Stream stream;
  if (stats != nullptr) {
    stream = stats->Aggregate({.first_frame = num_frames / 2});
    if (field_trial::IsEnabled("WebRTC-QuickPerfTest")) {
      EXPECT_NEAR(stream.bitrate_mismatch_pct.GetAverage(), 0, 10);
      EXPECT_NEAR(stream.framerate_mismatch_pct.GetAverage(), 0, 10);
    }
  }

  stream.LogMetrics(
      GetGlobalMetricsLogger(),
      ::testing::UnitTest::GetInstance()->current_test_info()->name(),
      /*metric_prefix=*/"",
      /*metadata=*/
      {{"codec_type", codec_type},
       {"codec_impl", codec_impl},
       {"video_name", video_info.file_name},
       {"rate_profile", std::to_string(bitrate_kbps.first) + "," +
                            std::to_string(bitrate_kbps.second)}});
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BitrateAdaptationTest,
    Combine(Values("AV1", "VP9", "VP8", "H264", "H265"),
#if defined(WEBRTC_ANDROID)
            Values("builtin", "mediacodec"),
#else
            Values("builtin"),
#endif
            Values(kSourceVideos.at("FourPeople_1280x720_30")),
            Values(std::pair(1024, 512), std::pair(512, 1024))),
    BitrateAdaptationTest::ToString);

#if 0

class FramerateAdaptationTest
    : public ::testing::TestWithParam<std::tuple</*codec_type=*/std::string,
                                                 /*codec_impl=*/std::string,
                                                 VideoInfo,
                                                 std::pair<double, double>>> {
 public:
  static std::string ToString(
      const ::testing::TestParamInfo<FramerateAdaptationTest::ParamType>&
          info) {
    auto [codec_type, codec_impl, video_info, framerate_fps] = info.param;
    return std::string(
        codec_type + codec_impl + video_info.file_name +
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
    if (absl::GetFlag(FLAGS_webrtc_quick_perf_test)) {
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
                                 Values(kSourceVideos.at("FourPeople_1280x720_30")),
                                 Values(std::pair(30, 15), std::pair(15, 30))),
                         FramerateAdaptationTest::ToString);
#endif
}  // namespace test

}  // namespace webrtc
