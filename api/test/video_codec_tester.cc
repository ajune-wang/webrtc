/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/video_codec_tester.h"

#include <set>

#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/codecs/av1/av1_svc_config.h"
#include "modules/video_coding/codecs/vp9/svc_config.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {
namespace {
using FramesSettings = VideoCodecTester::FramesSettings;
using LayerSettings = VideoCodecTester::EncodingSettings::LayerSettings;
using LayerId = VideoCodecTester::EncodingSettings::LayerId;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

const std::set<ScalabilityMode> kFullSvcScalabilityModes{
    ScalabilityMode::kL2T1,  ScalabilityMode::kL2T1h, ScalabilityMode::kL2T2,
    ScalabilityMode::kL2T2h, ScalabilityMode::kL2T3,  ScalabilityMode::kL2T3h,
    ScalabilityMode::kL3T1,  ScalabilityMode::kL3T1h, ScalabilityMode::kL3T2,
    ScalabilityMode::kL3T2h, ScalabilityMode::kL3T3,  ScalabilityMode::kL3T3h};

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

  if (layer_bitrates_kbps.size() > 1 ||
      (num_spatial_layers == 1 && num_temporal_layers == 1)) {
    RTC_CHECK(static_cast<int>(layer_bitrates_kbps.size()) ==
              num_spatial_layers * num_temporal_layers)
        << "bitrates must be provided for all layers";

    std::vector<DataRate> layer_bitrates;
    std::transform(layer_bitrates_kbps.begin(), layer_bitrates_kbps.end(),
                   std::back_inserter(layer_bitrates),
                   [](const auto& bitrate_kbps) {
                     return DataRate::KilobitsPerSec(bitrate_kbps);
                   });
    return std::make_tuple(layer_bitrates, scalability_mode);
  }

  VideoCodec vc;
  vc.codecType = PayloadStringToCodecType(codec_type);
  vc.width = width;
  vc.height = height;
  vc.startBitrate = layer_bitrates_kbps[0];
  vc.maxBitrate = layer_bitrates_kbps[0];
  vc.minBitrate = 0;
  vc.maxFramerate = static_cast<uint32_t>(framerate_fps);
  vc.numberOfSimulcastStreams = 0;
  vc.mode = webrtc::VideoCodecMode::kRealtimeVideo;
  vc.SetScalabilityMode(scalability_mode);

  switch (vc.codecType) {
    case kVideoCodecVP8:
      // TODO(webrtc:14852): Configure S>1 modes.
      *(vc.VP8()) = VideoEncoder::GetDefaultVp8Settings();
      vc.VP8()->SetNumberOfTemporalLayers(num_temporal_layers);
      vc.simulcastStream[0].width = vc.width;
      vc.simulcastStream[0].height = vc.height;
      break;
    case kVideoCodecVP9: {
      *(vc.VP9()) = VideoEncoder::GetDefaultVp9Settings();
      vc.VP9()->SetNumberOfTemporalLayers(num_temporal_layers);
      const std::vector<SpatialLayer> spatialLayers = GetVp9SvcConfig(vc);
      for (size_t i = 0; i < spatialLayers.size(); ++i) {
        vc.spatialLayers[i] = spatialLayers[i];
        vc.spatialLayers[i].active = true;
      }
    } break;
    case kVideoCodecAV1: {
      bool result =
          SetAv1SvcConfig(vc, num_spatial_layers, num_temporal_layers);
      RTC_CHECK(result) << "SetAv1SvcConfig failed";
    } break;
    case kVideoCodecH264: {
      *(vc.H264()) = VideoEncoder::GetDefaultH264Settings();
      vc.H264()->SetNumberOfTemporalLayers(num_temporal_layers);
    } break;
    case kVideoCodecH265:
      break;
    case kVideoCodecGeneric:
    case kVideoCodecMultiplex:
      RTC_CHECK_NOTREACHED();
  }

  if (*vc.GetScalabilityMode() != scalability_mode) {
    RTC_LOG(LS_WARNING) << "Scalability mode changed from "
                        << ScalabilityModeToString(scalability_mode) << " to "
                        << ScalabilityModeToString(*vc.GetScalabilityMode());
    num_spatial_layers =
        ScalabilityModeToNumSpatialLayers(*vc.GetScalabilityMode());
    num_temporal_layers =
        ScalabilityModeToNumTemporalLayers(*vc.GetScalabilityMode());
  }

  std::unique_ptr<VideoBitrateAllocator> bitrate_allocator =
      CreateBuiltinVideoBitrateAllocatorFactory()->CreateVideoBitrateAllocator(
          vc);

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

}  // namespace

DataRate VideoCodecTester::EncodingSettings::GetTargetBitrate(
    absl::optional<LayerId> layer_id) const {
  int base_sidx;
  if (layer_id.has_value()) {
    bool is_svc = kFullSvcScalabilityModes.count(scalability_mode);
    base_sidx = is_svc ? 0 : layer_id->spatial_idx;
  } else {
    int max_spatial_idx = ScalabilityModeToNumSpatialLayers(scalability_mode);
    int max_temporal_idx = ScalabilityModeToNumTemporalLayers(scalability_mode);
    layer_id = LayerId({.spatial_idx = max_spatial_idx - 1,
                        .temporal_idx = max_temporal_idx - 1});
    base_sidx = 0;
  }

  DataRate bitrate = DataRate::Zero();
  for (int sidx = base_sidx; sidx <= layer_id->spatial_idx; ++sidx) {
    for (int tidx = 0; tidx <= layer_id->temporal_idx; ++tidx) {
      auto layer_settings =
          layers_settings.find({.spatial_idx = sidx, .temporal_idx = tidx});
      RTC_CHECK(layer_settings != layers_settings.end())
          << "bitrate is not specified for layer sidx=" << sidx
          << " tidx=" << tidx;
      bitrate += layer_settings->second.bitrate;
    }
  }
  return bitrate;
}

Frequency VideoCodecTester::EncodingSettings::GetTargetFramerate(
    absl::optional<LayerId> layer_id) const {
  if (layer_id.has_value()) {
    auto layer_settings =
        layers_settings.find({.spatial_idx = layer_id->spatial_idx,
                              .temporal_idx = layer_id->temporal_idx});
    RTC_CHECK(layer_settings != layers_settings.end())
        << "framerate is not specified for layer sidx=" << layer_id->spatial_idx
        << " tidx=" << layer_id->temporal_idx;
    return layer_settings->second.framerate;
  }

  return layers_settings.rbegin()->second.framerate;
}

FramesSettings VideoCodecTester::CreateFramesSettings(
    std::string codec_type,
    std::string scalability_name,
    int width,
    int height,
    std::vector<int> layer_bitrates_kbps,
    double framerate_fps,
    int num_frames,
    uint32_t initial_timestamp_rtp) {
  auto [layer_bitrates, scalability_mode] =
      SplitBitrateAndUpdateScalabilityMode(
          codec_type, *ScalabilityModeFromString(scalability_name), width,
          height, layer_bitrates_kbps, framerate_fps);

  int num_spatial_layers = ScalabilityModeToNumSpatialLayers(scalability_mode);
  int num_temporal_layers =
      ScalabilityModeToNumTemporalLayers(scalability_mode);

  std::map<LayerId, LayerSettings> layers_settings;
  for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
    int layer_width = width >> (num_spatial_layers - sidx - 1);
    int layer_height = height >> (num_spatial_layers - sidx - 1);
    for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
      double layer_framerate_fps =
          framerate_fps / (1 << (num_temporal_layers - tidx - 1));
      layers_settings.emplace(
          LayerId{.spatial_idx = sidx, .temporal_idx = tidx},
          LayerSettings{
              .resolution = {.width = layer_width, .height = layer_height},
              .framerate = Frequency::MilliHertz(1000 * layer_framerate_fps),
              .bitrate = layer_bitrates[sidx * num_temporal_layers + tidx]});
    }
  }

  FramesSettings frames_settings;
  uint32_t timestamp_rtp = initial_timestamp_rtp;
  for (int frame_num = 0; frame_num < num_frames; ++frame_num) {
    frames_settings.emplace(
        timestamp_rtp,
        EncodingSettings{.sdp_video_format = SdpVideoFormat(codec_type),
                         .scalability_mode = scalability_mode,
                         .layers_settings = layers_settings});

    timestamp_rtp += k90kHz / Frequency::MilliHertz(1000 * framerate_fps);
  }

  return frames_settings;
}

}  // namespace test
}  // namespace webrtc