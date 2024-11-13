/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/h265/h265_svc_config.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "modules/video_coding/svc/scalability_mode_util.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {

std::optional<ScalabilityMode> BuildTemporalScalabilityMode(
    int num_temporal_layers) {
  char name[20];
  rtc::SimpleStringBuilder ss(name);
  ss << "L1" << "T" << num_temporal_layers;

  return ScalabilityModeFromString(name);
}

}  // namespace

void SetH265SvcConfig(VideoCodec& video_codec, int num_temporal_layers) {
  RTC_DCHECK_EQ(video_codec.codecType, kVideoCodecH265);

  std::optional<ScalabilityMode> scalability_mode =
      video_codec.GetScalabilityMode();
  if (!scalability_mode.has_value()) {
    scalability_mode = BuildTemporalScalabilityMode(num_temporal_layers);
    if (!scalability_mode) {
      RTC_LOG(LS_WARNING) << "Scalability mode is not set, using 'L1T1'.";
      scalability_mode = ScalabilityMode::kL1T1;
    }
  }
  SpatialLayer& spatial_layer = video_codec.spatialLayers[0];
  spatial_layer.active = true;
  spatial_layer.width = video_codec.width;
  spatial_layer.height = video_codec.height;
  spatial_layer.maxFramerate = video_codec.maxFramerate;
  spatial_layer.numberOfTemporalLayers = num_temporal_layers;
  spatial_layer.minBitrate = video_codec.minBitrate;
  spatial_layer.maxBitrate = video_codec.maxBitrate;
  spatial_layer.targetBitrate =
      (video_codec.minBitrate + video_codec.maxBitrate) / 2;
}

}  // namespace webrtc
