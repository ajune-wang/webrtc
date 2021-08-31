/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_SPATIAL_LAYER_H_
#define API_VIDEO_CODECS_SPATIAL_LAYER_H_

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct SpatialLayer {
  bool operator==(const SpatialLayer& other) const;
  bool operator!=(const SpatialLayer& other) const { return !(*this == other); }

  unsigned short width;   // NOLINT(runtime/int)
  unsigned short height;  // NOLINT(runtime/int)
  float maxFramerate;     // fps.
  unsigned char numberOfTemporalLayers;
  unsigned int maxBitrate;     // kilobits/sec.
  unsigned int targetBitrate;  // kilobits/sec.
  unsigned int minBitrate;     // kilobits/sec.
  unsigned int qpMax;          // minimum quality
  bool active;                 // encoded and sent.
};

// Returns number of spatial layers that is used for the specified scalability
// mode. See https://w3c.github.io/webrtc-svc/#scalabilitymodes* for a
// specification of valid values for |scalability_mode|. absl::nullopt is
// returned if the specified scalability mode cannot be interpreted.
RTC_EXPORT absl::optional<int> NumSpatialLayersInScalabilityMode(
    absl::string_view scalability_mode);

}  // namespace webrtc
#endif  // API_VIDEO_CODECS_SPATIAL_LAYER_H_
