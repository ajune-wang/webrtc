/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/video_codec_test_common.h"

#include <set>

#include "modules/video_coding/svc/scalability_mode_util.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {
namespace {
const std::set<ScalabilityMode> kFullSvcScalabilityModes{
    ScalabilityMode::kL2T1,  ScalabilityMode::kL2T1h, ScalabilityMode::kL2T2,
    ScalabilityMode::kL2T2h, ScalabilityMode::kL2T3,  ScalabilityMode::kL2T3h,
    ScalabilityMode::kL3T1,  ScalabilityMode::kL3T1h, ScalabilityMode::kL3T2,
    ScalabilityMode::kL3T2h, ScalabilityMode::kL3T3,  ScalabilityMode::kL3T3h};
}  // namespace

DataRate EncodingSettings::GetTargetBitrate(
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

Frequency EncodingSettings::GetTargetFramerate(
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

}  // namespace test
}  // namespace webrtc
