/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_LAYERS_ALLOCATION_H_
#define API_VIDEO_VIDEO_LAYERS_ALLOCATION_H_

#include <cstdint>
#include "absl/container/inlined_vector.h"

namespace webrtc {

// This struct contains additional stream-level information needed by
// SFUs to make relay decisions of RTP streams.
struct VideoLayersAllocation {
  static constexpr int kMaxSpatialIds = 4;
  static constexpr int kMaxTemporalIds = 4;

  struct ResolutionAndFrameRate {
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t frame_rate = 0;

    bool operator==(const ResolutionAndFrameRate& that) const {
      return width == that.width && height == that.height &&
             frame_rate == that.frame_rate;
    }
  };

  // Number of currently active spatial layers.
  // int active_spatial_layers = 0;
  int simulcast_id = 0;

  // Target bitrate per spatial and temporal layer in bps.
  absl::InlinedVector<uint32_t, kMaxTemporalIds> target_bitrate[kMaxSpatialIds];

  // Resolution and frame rate per spatial layer. Ordered from lowest spatial id
  // to to highest.
  absl::InlinedVector<ResolutionAndFrameRate, kMaxSpatialIds>
      resolution_and_frame_rate;

  bool Equals(const VideoLayersAllocation& that) const {
    bool res = simulcast_id == that.simulcast_id &&
               resolution_and_frame_rate == that.resolution_and_frame_rate;
    if (!res)
      return res;
    for (int i = 0; i < kMaxSpatialIds; ++i) {
      res &= target_bitrate[i] == that.target_bitrate[i];
    }
    return res;
  }
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_LAYERS_ALLOCATION_H_
