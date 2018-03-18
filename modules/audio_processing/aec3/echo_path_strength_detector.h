/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_STRENGTH_DETECTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_STRENGTH_DETECTOR_H_

#include <limits>

#include "api/optional.h"
#include "modules/audio_processing/aec3/delay_estimate.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class EchoPathStrengthDetector {
 public:
  enum class Strength { kNormal, kZero };

  EchoPathStrengthDetector();
  void Reset();
  void Update(const rtc::Optional<DelayEstimate>& delay_estimate,
              bool active_render,
              bool consistent_filter_estimate);
  Strength GetStrength() const { return echo_path_strength_; }

 private:
  size_t render_activity_counter_ = 0;
  size_t blocks_since_consistent_filter_estimate_ = 0;
  Strength echo_path_strength_ = Strength::kNormal;

  RTC_DISALLOW_COPY_AND_ASSIGN(EchoPathStrengthDetector);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_STRENGTH_DETECTOR_H_
