/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSOR_GAIN_DELAY_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSOR_GAIN_DELAY_BUFFER_H_

#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

// Delays echo suppressor gains and adjusts the gains according to the delay.
class SuppressorGainDelayBuffer {
 public:
  explicit SuppressorGainDelayBuffer(float delay_ms);

  // Delays the low- and high-band gains using the specified delay.
  void Delay(rtc::ArrayView<float, kFftLengthBy2Plus1> low_band_gains,
             float* high_bands_gain);

 private:
  const int delay_blocks_;
  const float fractional_delay_blocks_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> low_band_gain_buffer_;
  std::vector<float> high_bands_gain_buffer_;
  bool all_buffer_populated_ = false;
  size_t last_insert_index_ = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSOR_GAIN_DELAY_BUFFER_H_
