/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_REORDER_OPTIMIZER_H_
#define MODULES_AUDIO_CODING_NETEQ_REORDER_OPTIMIZER_H_

#include "absl/types/optional.h"
#include "modules/audio_coding/neteq/histogram.h"

namespace webrtc {

// Estimates the probability of receiving reordered packets.
// The optimal delay is decided by balancing the cost of increasing the delay
// against the probability of missing a reordered packet, resulting in a loss.
// The balance is decided using the `ms_per_loss_percent` parameter.
class ReorderOptimizer {
 public:
  ReorderOptimizer(int forget_factor,
                   int ms_per_loss_percent,
                   absl::optional<int> start_forget_weight);

  void Update(int relative_delay_ms, bool reordered);

  absl::optional<int> GetOptimalDelayMs() const { return optimal_delay_ms_; }

  void Reset();

 private:
  int MinimizeCostFunction() const;

  Histogram histogram_;
  const int ms_per_loss_percent_;
  absl::optional<int> optimal_delay_ms_;
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_REORDER_OPTIMIZER_H_
