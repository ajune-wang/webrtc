/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_channels_selector/average_energy_estimator.h"

#include <math.h>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {

constexpr int kNumChannelsToReserve = 2;

float ComputeChannelEnergy(const AudioBuffer& audio_buffer,
                           int channel,
                           float dc_level) {
  RTC_DCHECK_LT(channel, audio_buffer.num_channels());
  rtc::ArrayView<const float> channel_data(
      &audio_buffer.channels_const()[channel][0], audio_buffer.num_frames());
  float energy = 0.0f;
  for (int k = 0; k < static_cast<int>(channel_data.size()); ++k) {
    const float sample_minus_dc = channel_data[k] - dc_level;
    energy += sample_minus_dc * sample_minus_dc;
  }

  return energy;
}

}  // namespace

AverageEnergyEstimator::AverageEnergyEstimator() {
  average_energy_in_channels_.reserve(kNumChannelsToReserve);
}

void AverageEnergyEstimator::Update(const AudioBuffer& audio_buffer,
                                    rtc::ArrayView<const float> dc_levels) {
  RTC_DCHECK_EQ(average_energy_in_channels_.size(),
                audio_buffer.num_channels());
  RTC_DCHECK_EQ(dc_levels.size(), audio_buffer.num_channels());

  for (int channel = 0; channel < static_cast<int>(audio_buffer.num_channels());
       ++channel) {
    const float energy =
        ComputeChannelEnergy(audio_buffer, channel, dc_levels[channel]);

    constexpr float kForgettingFactor = 0.01f;
    average_energy_in_channels_[channel] +=
        kForgettingFactor * (energy - average_energy_in_channels_[channel]);
  }
}

void AverageEnergyEstimator::Reset() {
  std::fill(average_energy_in_channels_.begin(),
            average_energy_in_channels_.end(), 0.0f);
}

void AverageEnergyEstimator::SetAudioProperties(
    const AudioBuffer& audio_buffer) {
  average_energy_in_channels_.resize(audio_buffer.num_channels(), 0.0f);
}

}  // namespace webrtc
