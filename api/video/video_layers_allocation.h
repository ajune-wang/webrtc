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
    friend bool operator==(const ResolutionAndFrameRate& lhs,
                           const ResolutionAndFrameRate& rhs) {
      return lhs.width == rhs.width && lhs.height == rhs.height &&
             lhs.frame_rate == rhs.frame_rate;
    }

    friend bool operator!=(const ResolutionAndFrameRate& lhs,
                           const ResolutionAndFrameRate& rhs) {
      return !(lhs == rhs);
    }

    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t frame_rate = 0;
  };

  friend bool operator==(const VideoLayersAllocation& lhs,
                         const VideoLayersAllocation& rhs) {
    if (lhs.rtp_stream_index != rhs.rtp_stream_index ||
        lhs.resolution_and_frame_rate != rhs.resolution_and_frame_rate) {
      return false;
    }
    for (int i = 0; i < kMaxSpatialIds; ++i) {
      if (lhs.target_bitrate[i] != rhs.target_bitrate[i])
        return false;
    }
    return true;
  }

  friend bool operator!=(const VideoLayersAllocation& lhs,
                         const VideoLayersAllocation& rhs) {
    return !(lhs == rhs);
  }

  // Index of the simulcast encoding this allocation is sent on. When all layers
  // are sent over same ssrc/rtp stream, this value is 0.
  int rtp_stream_index = 0;

  // Target bitrate per decode target in bps, identified by spatial and temporal
  // layer.
  absl::InlinedVector<uint32_t, kMaxTemporalIds> target_bitrate[kMaxSpatialIds];

  // Resolution and frame rate per spatial layer. Ordered from lowest spatial id
  // to to highest.
  absl::InlinedVector<ResolutionAndFrameRate, kMaxSpatialIds>
      resolution_and_frame_rate;
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_LAYERS_ALLOCATION_H_
