/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/power_based_nearend_detector.h"

#include <numeric>

namespace webrtc {
PowerBasedNearendDetector::PowerBasedNearendDetector(
    const EchoCanceller3Config::Suppressor::PowerBasedNearendDetection& config,
    size_t num_capture_channels)
    : config_(config),
      num_capture_channels_(num_capture_channels),
      nearend_smoothers_(num_capture_channels_,
                         aec3::MovingAverage(kFftLengthBy2Plus1,
                                             config_.nearend_average_blocks)),
      one_over_num_bands_region1_(1.f / (config_.band_range_region1[1] -
                                         config_.band_range_region1[0] + 1)),
      one_over_num_bands_region2_(1.f / (config_.band_range_region2[1] -
                                         config_.band_range_region2[0] + 1)) {}

void PowerBasedNearendDetector::Update(
    rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>>
        nearend_spectrum,
    rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>>
        residual_echo_spectrum,
    rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>>
        comfort_noise_spectrum,
    bool initial_state) {
  nearend_state_ = false;
  for (size_t ch = 0; ch < num_capture_channels_; ++ch) {
    const std::array<float, kFftLengthBy2Plus1>& noise =
        comfort_noise_spectrum[ch];
    std::array<float, kFftLengthBy2Plus1> nearend;
    nearend_smoothers_[ch].Average(nearend_spectrum[ch], nearend);

    // Noise power of the first region.
    float noise_power =
        std::accumulate(noise.begin() + config_.band_range_region1[0],
                        noise.begin() + config_.band_range_region1[1] + 1,
                        0.f) *
        one_over_num_bands_region1_;

    // Nearend power of the first region.
    float nearend_power_reg1 =
        std::accumulate(nearend.begin() + config_.band_range_region1[0],
                        nearend.begin() + config_.band_range_region1[1] + 1,
                        0.f) *
        one_over_num_bands_region1_;

    // Nearend power of the second region.
    float nearend_power_reg2 =
        std::accumulate(nearend.begin() + config_.band_range_region2[0],
                        nearend.begin() + config_.band_range_region2[1] + 1,
                        0.f) *
        one_over_num_bands_region2_;

    // One channel is sufficient to trigger nearend state.
    nearend_state_ =
        nearend_state_ ||
        (nearend_power_reg1 < config_.nearend_threshold * nearend_power_reg2 &&
         (nearend_power_reg1 / noise_power > config_.snr_threshold));
  }
}
}  // namespace webrtc
