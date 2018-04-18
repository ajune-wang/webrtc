/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_BITRATE_ALLOCATION_H_
#define API_VIDEO_BITRATE_ALLOCATION_H_

#include <string.h>
#include <string>
#include <vector>

#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

enum { kMaxSimulcastStreams = 4 };
enum { kMaxSpatialLayers = 5 };
enum { kMaxTemporalStreams = 4 };

class VideoBitrateAllocation {
 public:
  static const uint32_t kMaxBitrateBps;
  VideoBitrateAllocation();

  bool SetBitrate(size_t spatial_index,
                  size_t temporal_index,
                  uint32_t bitrate_bps);

  bool HasBitrate(size_t spatial_index, size_t temporal_index) const;

  uint32_t GetBitrate(size_t spatial_index, size_t temporal_index) const;

  // Whether the specific spatial layers has the bitrate set in any of its
  // temporal layers.
  bool IsSpatialLayerUsed(size_t spatial_index) const;

  // Get the sum of all the temporal layer for a specific spatial layer.
  uint32_t GetSpatialLayerSum(size_t spatial_index) const;

  // Sum of bitrates of temporal layers, from layer 0 to |temporal_index|
  // inclusive, of specified spatial layer |spatial_index|. Bitrates of lower
  // spatial layers are not included.
  uint32_t GetTemporalLayerSum(size_t spatial_index,
                               size_t temporal_index) const;

  // Returns a vector of the temporal layer bitrates for the specific spatial
  // layer. Length of the returned vector is cropped to the highest temporal
  // layer with a defined bitrate.
  std::vector<uint32_t> GetTemporalLayerAllocation(size_t spatial_index) const;

  uint32_t get_sum_bps() const { return sum_; }  // Sum of all bitrates.
  uint32_t get_sum_kbps() const {
    // Round down to not exceed the allocated bitrate.
    return sum_ / 1000;
  }

  inline bool operator==(const VideoBitrateAllocation& other) const {
    return memcmp(bitrates_, other.bitrates_, sizeof(bitrates_)) == 0;
  }
  inline bool operator!=(const VideoBitrateAllocation& other) const {
    return !(*this == other);
  }

  // Expensive, please use only in tests.
  std::string ToString() const;

 private:
  uint32_t sum_;
  uint32_t bitrates_[kMaxSpatialLayers][kMaxTemporalStreams];
  bool has_bitrate_[kMaxSpatialLayers][kMaxTemporalStreams];
};

// TODO(sprang): Remove this when downstream projects have been updated.
using BitrateAllocation = VideoBitrateAllocation;

}  // namespace webrtc

#endif  // API_VIDEO_BITRATE_ALLOCATION_H_
