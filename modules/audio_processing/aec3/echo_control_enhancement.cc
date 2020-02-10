/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/echo_control_enhancement.h"

#include <algorithm>

#include "api/array_view.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

std::unique_ptr<SuppressorGainDelayBuffer> ConditionallyCreateGainDelayBuffer(
    const AudioEnhancer* enhancer) {
  if (enhancer && enhancer->AlgorithmicDelayInMs() != 0.f) {
    return std::make_unique<SuppressorGainDelayBuffer>(
        enhancer->AlgorithmicDelayInMs());
  }
  return nullptr;
}

std::unique_ptr<BlockDelayBuffer> ConditionallyCreateSignalDelayer(
    const AudioEnhancer* enhancer,
    size_t num_channels,
    size_t num_bands) {
  if (!enhancer->ModifiesInputSignal() && enhancer->AlgorithmicDelayInMs()) {
    size_t delay_samples =
        static_cast<size_t>(16 * enhancer->AlgorithmicDelayInMs());
    return std::make_unique<BlockDelayBuffer>(num_channels, num_bands,
                                              kBlockSize, delay_samples);
  }
  return nullptr;
}

}  // namespace

EchoControlEnhancement::EchoControlEnhancement(size_t num_capture_channels,
                                               size_t num_bands,
                                               AudioEnhancer* enhancer)
    : num_capture_channels_(num_capture_channels),
      Y_re_(num_capture_channels),
      Y_im_(num_capture_channels),
      enhancer_(enhancer),
      gain_delay_buffer_(ConditionallyCreateGainDelayBuffer(enhancer_)),
      signal_delay_buffer_(
          ConditionallyCreateSignalDelayer(enhancer_,
                                           num_capture_channels_,
                                           num_bands)) {}

EchoControlEnhancement::~EchoControlEnhancement() = default;

void EchoControlEnhancement::Enhance(
    bool use_linear_filter_output,
    rtc::ArrayView<const std::array<float, kFftLengthBy2>> linear_filter_output,
    std::vector<std::vector<std::vector<float>>>* y,
    rtc::ArrayView<FftData> Y,
    rtc::ArrayView<float, kFftLengthBy2Plus1> low_band_noise_suppression_gains,
    float* high_bands_noise_suppression_gain,
    rtc::ArrayView<float, kFftLengthBy2Plus1> low_band_echo_suppression_gains,
    float* high_bands_echo_suppression_gain) {
  RTC_DCHECK(enhancer_);
  RTC_DCHECK(high_bands_echo_suppression_gain);

  if (use_linear_filter_output) {
    for (size_t ch = 0; ch < num_capture_channels_; ++ch) {
      std::copy(linear_filter_output[ch].begin(),
                linear_filter_output[ch].end(), (*y)[0][ch].begin());
    }
  }

  RTC_DCHECK_EQ(num_capture_channels_, Y_re_.size());
  RTC_DCHECK_EQ(num_capture_channels_, Y_im_.size());
  for (size_t ch = 0; ch < num_capture_channels_; ++ch) {
    Y_re_[ch] = &Y[ch].re;
    Y_im_[ch] = &Y[ch].im;
  }

  std::array<float, 65> unscaled_low_band_noise_suppression_gains;
  unscaled_low_band_noise_suppression_gains.fill(1.f);
  float unscaled_high_bands_noise_suppression_gain = 1.f;
  std::array<float, 65> level_adjustment_gains;
  level_adjustment_gains.fill(1.f);
  float high_bands_level_adjustment_gain = 1.f;

  enhancer_->Process(
      Y_re_, Y_im_, y, &unscaled_low_band_noise_suppression_gains,
      &unscaled_high_bands_noise_suppression_gain, &level_adjustment_gains,
      &high_bands_level_adjustment_gain);

  if (enhancer_->AlgorithmicDelayInMs() != 0.f) {
    RTC_DCHECK(gain_delay_buffer_);

    if (!enhancer_->ModifiesInputSignal()) {
      RTC_DCHECK(signal_delay_buffer_);
      signal_delay_buffer_->DelaySignal(y);
    }

    gain_delay_buffer_->Delay(low_band_echo_suppression_gains,
                              high_bands_echo_suppression_gain);
  }

  // Adjust gains.
  for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
    RTC_DCHECK_GE(1.f, unscaled_low_band_noise_suppression_gains[k]);
    low_band_noise_suppression_gains[k] =
        unscaled_low_band_noise_suppression_gains[k] *
        level_adjustment_gains[k];
    low_band_echo_suppression_gains[k] *= level_adjustment_gains[k];
  }
  RTC_DCHECK_GE(1.f, unscaled_high_bands_noise_suppression_gain);
  *high_bands_noise_suppression_gain =
      unscaled_high_bands_noise_suppression_gain *
      high_bands_level_adjustment_gain;
  *high_bands_echo_suppression_gain *= high_bands_level_adjustment_gain;
}

}  // namespace webrtc
