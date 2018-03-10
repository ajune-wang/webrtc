/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_CLOCK_DRIFT_DETECTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_CLOCK_DRIFT_DETECTOR_H_

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/optional.h"
#include "modules/audio_processing/aec3/delay_estimate.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ClockDriftDetector {
 public:
  explicit ClockDriftDetector(const EchoCanceller3Config& config);
  void Reset();
  void Update(rtc::ArrayView<const float> filter,
              const rtc::Optional<DelayEstimate>& delay_estimate,
              bool converged_filter);
  int HasClockDrift() const { return clock_drift_; }

 private:
  const bool clock_drift_flagged_;
  bool clock_drift_ = false;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(ClockDriftDetector);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_CLOCK_DRIFT_DETECTOR_H_
