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
  Reset();
}

void EchoPathStrengthDetector::Reset() {
  render_activity_since_converged_filter_ =
      std::numeric_limits<std::size_t>::max();
  old_delay_ = 0;
  convergence_counter_ = 0;
  capture_counter_ = 0;
}

void EchoPathStrengthDetector::Update(
    const rtc::Optional<DelayEstimate>& delay_estimate,
    bool active_render,
    bool good_filter_estimate,
    bool converged_filter) {
  ++capture_counter_;

  // Flag for stable delay.
  bool stable_delay_estimate =
      delay_estimate &&
      (delay_estimate->delay == old_delay_ &&
       delay_estimate->blocks_since_last_change > 4 * kNumBlocksPerSecond &&
       delay_estimate->blocks_since_last_update <
           delay_estimate->blocks_since_last_change);

  // Flag for filter convergence.
  convergence_counter_ = converged_filter ? convergence_counter_ + 1 : 0;
  bool good_convergence = convergence_counter_ > 1;
  render_activity_since_converged_filter_ =
      good_convergence
          ? 0
          : render_activity_since_converged_filter_ + (active_render ? 1 : 0);
  bool converged_filter_seen =
      render_activity_since_converged_filter_ < 120 * kNumBlocksPerSecond;

  bool sufficient_capture_to_converge = capture_counter_ > 10 * kNumBlocksPerSecond;
  render_activity_counter_ += active_render ? 1 : 0;
  bool sufficient_render_to_converge = render_activity_counter_ > 5 * kNumBlocksPerSecond;
  if (!converged_filter_seen && !stable_delay_estimate &&
      sufficient_capture_to_converge  && sufficient_render_to_converge) {
    if (!good_filter_estimate) {
      echo_path_strength_ = Strength::kZero;
    }
    else {
      echo_path_strength_ = Strength::kWeak;
    }
  } else {
    echo_path_strength_ = Strength::kNormal;
  }

  if (delay_estimate) {
    old_delay_ = delay_estimate->delay;
  }
}

}  // namespace webrtc
