/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/clock_drift_detector.h"

#include "api/array_view.h"

namespace webrtc {

ClockDriftDetector::ClockDriftDetector(const EchoCanceller3Config& config)
    : clock_drift_flagged_(false) {
  Reset();
}

void ClockDriftDetector::Reset() {
  clock_drift_ = false;
}

void ClockDriftDetector::Update(
    rtc::ArrayView<const float> filter,
    const rtc::Optional<DelayEstimate>& delay_estimate,
    bool converged_filter) {
  clock_drift_ = clock_drift_flagged_;
}

}  // namespace webrtc
