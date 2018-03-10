/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_saturation_detector.h"

#include <math.h>

#include <numeric>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"

namespace webrtc {

EchoSaturationDetector::EchoSaturationDetector(
    const EchoCanceller3Config& config)
    : can_saturate_(config.ep_strength.echo_can_saturate) {
  Reset();
}

void EchoSaturationDetector::Reset() {
  echo_saturation_ = false;
  blocks_since_last_saturation_ = std::numeric_limits<std::size_t>::max();
  echo_path_gain_ = 160;
}

void EchoSaturationDetector::Update(rtc::ArrayView<const float> x_aligned,
                                    bool saturated_capture,
                                    const rtc::Optional<float>& echo_path_gain,
                                    bool good_filter_estimate) {
  if (!can_saturate_) {
    echo_saturation_ = false;
    return;
  }

  RTC_DCHECK_LT(0, x_aligned.size());
  const float x_max =
      fabs(*std::max_element(x_aligned.begin(), x_aligned.end(),
                             [](float a, float b) { return a * a < b * b; }));

  if (good_filter_estimate && echo_path_gain) {
    echo_path_gain_ = *echo_path_gain;
  }

  constexpr float kMargin = 10.f;
  bool potentially_saturating_echo = kMargin * echo_path_gain_ * x_max > 32000;

  blocks_since_last_saturation_ =
      potentially_saturating_echo && saturated_capture
          ? 0
          : blocks_since_last_saturation_ + 1;

  echo_saturation_ = blocks_since_last_saturation_ < 20;
}

}  // namespace webrtc
