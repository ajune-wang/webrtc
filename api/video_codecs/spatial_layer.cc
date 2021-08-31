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

#include "absl/types/optional.h"

namespace webrtc {

bool SpatialLayer::operator==(const SpatialLayer& other) const {
  return (width == other.width && height == other.height &&
          maxFramerate == other.maxFramerate &&
          numberOfTemporalLayers == other.numberOfTemporalLayers &&
          maxBitrate == other.maxBitrate &&
          targetBitrate == other.targetBitrate &&
          minBitrate == other.minBitrate && qpMax == other.qpMax &&
          active == other.active);
}

absl::optional<int> NumSpatialLayersInScalabilityMode(
    absl::string_view scalability_mode) {
  if (scalability_mode.size() >= 4 &&
      (scalability_mode[0] == 'L' || scalability_mode[0] == 'S') &&
      (scalability_mode[2] == 'T') && scalability_mode[1] >= '1' &&
      scalability_mode[1] <= '3') {
    return scalability_mode[1] - '0';
  }
  return absl::nullopt;
}

}  // namespace webrtc
