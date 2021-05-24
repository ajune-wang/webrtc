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

// A circular buffer to store frame-wise `Level` items (square average and peak
// value) for clipping prediction. The current implementation is not optimized
// for large buffer lengths.
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

  // If enough `Level` items have been stored, returns the partial average and
  // maximum value for `num_items` items at index `delay` and before.
  // `delay` limited to [0, M] and `num_items` to [1, N] where N + M is the
  // capacity of the buffer.
  absl::optional<Level> ComputePartialMetrics(int delay, int num_items) const;

 private:
  // TODO(bugs.webrtc.org/12774): Optimize partial computation for long buffers.
  // If enough `Level` items have been stored, returns the partial average for
  // `num_items` items at index `delay` and before. `delay` limited to [0, M]
  // and `num_items` to [1, N] where N + M is the  capacity of the buffer.
  absl::optional<float> ComputePartialAverage(int delay, int num_items) const;

  // TODO(bugs.webrtc.org/12774): Optimize partial computation for long buffers.
  // If enough `Level` items have been stored, returns the partial maximum
  // for `num_items` items at index `delay` and before. `delay` limited to
  // [0, M]  and `num_items` to [1, N] where N + M is the  capacity of the
  // buffer.
  absl::optional<float> ComputePartialMax(int delay, int num_items) const;

  int BufferMaxLength() const { return data_.size(); }

  int tail_;
  int size_;
  std::vector<Level> data_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_LEVEL_BUFFER_H_
