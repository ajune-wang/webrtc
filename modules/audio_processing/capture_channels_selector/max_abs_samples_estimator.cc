/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_channels_selector/max_abs_samples_estimator.h"

#include <math.h>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {

float GetMaxAbsSample(float dc_level, rtc::ArrayView<const float> audio) {
  float max_abs_sample = 0.0f;
  for (int k = 0; k < static_cast<int>(audio.size()); ++k) {
    max_abs_sample = std::max(max_abs_sample, fabs(audio[k] - dc_level));
  }
  return max_abs_sample;
}

constexpr int kNumChannelsToReserve = 2;

}  // namespace

MaxAbsSamplesEstimator::MaxAbsSamplesEstimator() {
  max_abs_samples_in_channels_.reserve(kNumChannelsToReserve);
}

void MaxAbsSamplesEstimator::Update(const AudioBuffer& audio_buffer,
                                    rtc::ArrayView<const float> dc_levels) {
  RTC_DCHECK_EQ(max_abs_samples_in_channels_.size(),
                audio_buffer.num_channels());
  RTC_DCHECK_EQ(dc_levels.size(), audio_buffer.num_channels());

  adjusted_for_dc_levels_ = true;

  for (int channel = 0; channel < static_cast<int>(audio_buffer.num_channels());
       ++channel) {
    rtc::ArrayView<const float> channel_data(
        &audio_buffer.channels_const()[channel][0], audio_buffer.num_frames());
    max_abs_samples_in_channels_[channel] =
        std::max(max_abs_samples_in_channels_[channel],
                 GetMaxAbsSample(dc_levels[channel], channel_data));
  }
}

void MaxAbsSamplesEstimator::Update(const AudioBuffer& audio_buffer) {
  RTC_DCHECK_EQ(max_abs_samples_in_channels_.size(),
                audio_buffer.num_channels());
  RTC_DCHECK(!adjusted_for_dc_levels_);

  for (int channel = 0; channel < static_cast<int>(audio_buffer.num_channels());
       ++channel) {
    rtc::ArrayView<const float> channel_data(
        &audio_buffer.channels_const()[channel][0], audio_buffer.num_frames());
    max_abs_samples_in_channels_[channel] =
        std::max(max_abs_samples_in_channels_[channel],
                 GetMaxAbsSample(0.0f, channel_data));
  }
}

void MaxAbsSamplesEstimator::AdjustForDcLevels(
    rtc::ArrayView<const float> dc_levels) {
  RTC_DCHECK_EQ(dc_levels.size(), max_abs_samples_in_channels_.size());
  for (int channel = 0; channel < static_cast<int>(dc_levels.size());
       ++channel) {
    max_abs_samples_in_channels_[channel] -= fabsf(dc_levels[channel]);
  }

  adjusted_for_dc_levels_ = true;
}

void MaxAbsSamplesEstimator::Reset() {
  adjusted_for_dc_levels_ = false;
  std::fill(max_abs_samples_in_channels_.begin(),
            max_abs_samples_in_channels_.end(), 0.0f);
}

void MaxAbsSamplesEstimator::SetAudioProperties(
    const AudioBuffer& audio_buffer) {
  max_abs_samples_in_channels_.resize(audio_buffer.num_channels(), 0.0f);
}

}  // namespace webrtc
