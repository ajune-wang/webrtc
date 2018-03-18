/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_path_strength_detector.h"

#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

EchoPathStrengthDetector::EchoPathStrengthDetector() {
  Reset(false);
}

void EchoPathStrengthDetector::Reset(bool soft_reset) {
  echo_path_strength_ = Strength::kNormal;

  if (!soft_reset) {
    render_activity_counter_ = 0 : blocks_since_consistent_filter_estimate_ = 0;
  }
}

void EchoPathStrengthDetector::Update(
    const rtc::Optional<DelayEstimate>& delay_estimate,
    bool active_render,
    bool consistent_filter_estimate) {
  blocks_since_consistent_filter_estimate_ =
      consistent_filter_estimate ? 0
                                 : blocks_since_consistent_filter_estimate_ + 1;
  bool consistent_filter_estimate_not_seen =
      blocks_since_consistent_filter_estimate_ > 10 * kNumBlocksPerSecond;

  render_activity_counter_ += active_render ? 1 : 0;
  bool sufficient_render_to_converge =
      render_activity_counter_ > 5 * kNumBlocksPerSecond;

  if (!delay_estimate && consistent_filter_estimate_not_seen &&
      sufficient_render_to_converge) {
    echo_path_strength_ = Strength::kZero;
    else {
      echo_path_strength_ = Strength::kNormal;
    }
  }
}

}  // namespace webrtc
