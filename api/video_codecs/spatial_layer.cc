/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/spatial_layer.h"

#include <string>

#include "absl/types/optional.h"

namespace webrtc {
namespace {
struct ScalabilityModeSpatialLayers {
  const char* name;
  int spatial_layers;
};

constexpr ScalabilityModeSpatialLayers kSvcSpatialLayers[] = {
    {"L1T2", 1},           {"L1T3", 1},           {"L2T1", 2},
    {"L2T2", 2},           {"L2T3", 2},           {"L2T1h", 2},
    {"L2T2h", 2},          {"L2T3h", 2},          {"S2T1", 2},
    {"S2T2", 2},           {"S2T3", 2},           {"S2T1h", 2},
    {"S2T2h", 2},          {"S2T3h", 2},          {"L3T1", 3},
    {"L3T2", 3},           {"L3T3", 3},           {"S3T1", 3},
    {"S3T2", 3},           {"S3T3", 3},           {"S3T1h", 3},
    {"S3T2h", 3},          {"S3T3h", 3},          {"L2T2_KEY", 3},
    {"L2T2_KEY_SHIFT", 3}, {"L2T3_KEY", 3},       {"L2T3_KEY_SHIFT", 3},
    {"L3T2_KEY", 3},       {"L3T2_KEY_SHIFT", 3}, {"L3T3_KEY", 3},
};

}  // namespace

bool SpatialLayer::operator==(const SpatialLayer& other) const {
  return (width == other.width && height == other.height &&
          maxFramerate == other.maxFramerate &&
          numberOfTemporalLayers == other.numberOfTemporalLayers &&
          maxBitrate == other.maxBitrate &&
          targetBitrate == other.targetBitrate &&
          minBitrate == other.minBitrate && qpMax == other.qpMax &&
          active == other.active);
}

absl::optional<int> ScalabilityModeToSpatialLayers(
    const std::string& scalability_mode) {
  for (const auto& entry : kSvcSpatialLayers) {
    if (entry.name == scalability_mode) {
      return entry.spatial_layers;
    }
  }
  return absl::nullopt;
}

}  // namespace webrtc
