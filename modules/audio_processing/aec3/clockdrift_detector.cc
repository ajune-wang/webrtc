/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/clockdrift_detector.h"

namespace webrtc {

ClockdriftDetector::ClockdriftDetector()
    : level_(ClockdriftLevel::kNone), stability_counter(0) {
  lag_history_.fill(0);
}

ClockdriftDetector::~ClockdriftDetector() = default;

void ClockdriftDetector::Update(int lag) {
  if (lag == lag_history_[0]) {
    // Reset clockdrift level if lag is stable for some time.
    if (++stability_counter > 7500)
      level_ = ClockdriftLevel::kNone;
    return;
  }

  stability_counter = 0;
  const int d1 = lag_history_[0] - lag;
  const int d2 = lag_history_[1] - lag;
  const int d3 = lag_history_[2] - lag;

  // Patterns recognized as positive clockdrift:
  // [lag-3], lag-2, lag-1, lag.
  // [lag-3], lag-1, lag-2, lag.
  const bool probable_drift_up =
      (d1 == -1 && d2 == -2) || (d1 == -2 && d2 == -1);
  const bool drift_up = probable_drift_up && d3 == -3;

  // Patterns recognized as negative clockdrift:
  // [lag+3], lag+2, lag+1, lag.
  // [lag+3], lag+1, lag+2, lag.
  const bool probable_drift_down = (d1 == 1 && d2 == 2) || (d1 == 2 && d2 == 1);
  const bool drift_down = probable_drift_down && d3 == 3;

  // Set clockdrift level.
  if (drift_up || drift_down) {
    level_ = ClockdriftLevel::kVerified;
  } else if ((probable_drift_up || probable_drift_down) &&
             level_ == ClockdriftLevel::kNone) {
    level_ = ClockdriftLevel::kProbable;
  }

  // Shift lag history one step.
  lag_history_[2] = lag_history_[1];
  lag_history_[1] = lag_history_[0];
  lag_history_[0] = lag;
}
}  // namespace webrtc
