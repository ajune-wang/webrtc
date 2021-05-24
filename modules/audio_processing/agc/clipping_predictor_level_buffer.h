/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_LEVEL_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_LEVEL_BUFFER_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"

namespace webrtc {

// A circular buffer to store frame-wise `Level` items for clipping prediction.
// The current implementation is not optimized for large buffer lengths.
class ClippingPredictorLevelBuffer {
 public:
  struct Level {
    float average;
    float max;
  };

  // ClippingPredictorLevelBuffer with capacity of `max_length`. The value of
  // `max_length` limited to [0, `kMaxAllowedBufferLength`].
  explicit ClippingPredictorLevelBuffer(int max_length);
  ~ClippingPredictorLevelBuffer() {}
  ClippingPredictorLevelBuffer(const ClippingPredictorLevelBuffer&) = delete;
  ClippingPredictorLevelBuffer& operator=(const ClippingPredictorLevelBuffer&) =
      delete;

  void Reset();
  int Size() const { return size_; }

  // Adds a `level` item into the circular buffer `data_`. Stores at most
  // `buffer_max_length_` items. If more items are added, adding a new item
  // replaces the least recently added item.
  void Push(Level level);

  // TODO(bugs.webrtc.org/12774): Optimize partial computation for long buffers.
  // If at least `num_items` items have been stored, returns the average and
  // maximum value for the most recent `num_items` pushed items from `delay`
  // to `delay` - `num_items` (`delay` equal to zero corresponds to the most
  // recently pushed item). The value of `delay` is limited to [0, M] and
  // `num_items` to [1, N] where N + M is the capacity of the buffer.
  absl::optional<Level> ComputePartialMetrics(int delay, int num_items) const;

 private:
  int BufferMaxLength() const { return data_.size(); }

  int tail_;
  int size_;
  std::vector<Level> data_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_LEVEL_BUFFER_H_
