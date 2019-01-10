/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_DECODED_FRAMES_HISTORY_H_
#define MODULES_VIDEO_CODING_UTILITY_DECODED_FRAMES_HISTORY_H_

#include <stdint.h>
#include <bitset>
#include <vector>

namespace webrtc {

class DecodedFramesHistory {
 public:
  // window_size - how much frames back to the past are actually remembered.
  explicit DecodedFramesHistory(size_t window_size);
  ~DecodedFramesHistory();
  // Called for each decoded frame. Assumes picture id's are non-decreasing.
  void InsertDecoded(int64_t picture_id, int spatial_id);
  // Query if the following (picture_id, spatial_id) pair was inserted before.
  // Should be at most less by window_size-1 than the last inserted picture id.
  bool WasDecoded(int64_t picture_id, int spatial_id);
  void Clear();

 private:
  struct LayerHistory {
    LayerHistory();
    ~LayerHistory();
    // Cyclic bitset buffer. Stores last known |window_size| bits.
    // last_stored_index is the last actually stored bit. Previous
    // |window_size-1| bits are also in the memory. Value for i-th bit is at
    // buffer[i % window_size].
    std::vector<bool> buffer;
    int64_t last_stored_index;
  };

  const int window_size_;
  std::vector<LayerHistory> layers_;
};

}  // namespace webrtc
#endif  // MODULES_VIDEO_CODING_UTILITY_DECODED_FRAMES_HISTORY_H_
