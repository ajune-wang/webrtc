/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_SATURATION_DETECTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_SATURATION_DETECTOR_H_

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/optional.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class EchoSaturationDetector {
 public:
  explicit EchoSaturationDetector(const EchoCanceller3Config& config);
  void Reset();
  void Update(rtc::ArrayView<const float> x_aligned,
              bool saturated_capture,
              const rtc::Optional<float>& echo_path_gain,
              bool good_filter_estimate);
  bool SaturationDetected() const { return echo_saturation_; }

 private:
  const bool can_saturate_;
  bool echo_saturation_ = false;
  size_t blocks_since_last_saturation_ = 0;
  float echo_path_gain_ = 160.f;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EchoSaturationDetector);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ECHO_SATURATION_DETECTOR_H_
