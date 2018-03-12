/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_model_selector.h"
#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

EchoModelSelector::EchoModelSelector() {
  Reset();
}

void EchoModelSelector::Reset() {
  blocks_since_converged_filter_ = std::numeric_limits<std::size_t>::max();
  linear_model_selected_ = false;
  diverge_counter_ = 0;
}

void EchoModelSelector::Update(bool echo_saturation,
                               bool converged_filter,
                               bool diverged_filter,
                               size_t blocks_with_proper_filter_adaptation,
                               size_t capture_blocks_counter) {
  bool filter_has_had_time_to_converge =
      blocks_with_proper_filter_adaptation >= 1.5f * kNumBlocksPerSecond;

  blocks_since_converged_filter_ =
      converged_filter ? 0 : blocks_since_converged_filter_ + 1;
  bool recently_converged_filter =
      blocks_since_converged_filter_ < 30 * kNumBlocksPerSecond;

  diverge_counter_ = diverged_filter ? diverge_counter_ + 1 : 0;
  if (diverge_counter_ > 2) {
    blocks_since_converged_filter_ = std::numeric_limits<std::size_t>::max();
  }

  bool startup_phase_ended = capture_blocks_counter >= kNumBlocksPerSecond;

  linear_model_selected_ = !echo_saturation;
  linear_model_selected_ = linear_model_selected_ && recently_converged_filter;
  linear_model_selected_ =
      linear_model_selected_ && filter_has_had_time_to_converge;
  linear_model_selected_ = linear_model_selected_ && startup_phase_ended;
  linear_model_selected_ = linear_model_selected_ && diverge_counter_ < 4;
}

}  // namespace webrtc
