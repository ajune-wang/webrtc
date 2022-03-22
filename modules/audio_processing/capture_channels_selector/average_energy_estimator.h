/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_AVERAGE_ENERGY_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_AVERAGE_ENERGY_ESTIMATOR_H_

#include <stddef.h>

#include <vector>

#include "modules/audio_processing/audio_buffer.h"

namespace webrtc {

// Estimates the average energies for 10 ms frames of the channels in an
// `AudioBuffer`.
class AverageEnergyEstimator {
 public:
  AverageEnergyEstimator();
  AverageEnergyEstimator(const AverageEnergyEstimator&) = delete;
  AverageEnergyEstimator& operator=(const AverageEnergyEstimator&) = delete;

  // Updates the estimates of the average energies for 10 ms frames and for each
  // channel based on the content in `audio_buffer`. Any dc-levels in
  // `dc_levels` are subtracted before the estimation.
  void Update(const AudioBuffer& audio_buffer,
              rtc::ArrayView<const float> dc_levels);

  // Resets the estimates.
  void Reset();

  // Specifies the audio properties to use to match that of 'audio_buffer`.
  void SetAudioProperties(const AudioBuffer& audio_buffer);

  // Returns the energy estimates.
  const std::vector<float>& GetChannelEnergies() const {
    return average_energy_in_channels_;
  }

 private:
  std::vector<float> average_energy_in_channels_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_CAPTURE_CHANNELS_SELECTOR_AVERAGE_ENERGY_ESTIMATOR_H_
