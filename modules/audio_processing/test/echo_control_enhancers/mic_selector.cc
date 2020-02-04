/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/echo_control_enhancers/mic_selector.h"

#include <algorithm>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {}  // namespace

MicSelector::MicSelector(size_t num_input_channels)
    : average_mic_powers_(num_input_channels, 0.f) {}

void MicSelector::Process(const std::vector<std::array<float, 65>*>& X0_fft_re,
                          const std::vector<std::array<float, 65>*>& X0_fft_im,
                          std::vector<std::vector<std::vector<float>>>* x,
                          std::array<float, 65>* denoising_gains,
                          float* high_bands_denoising_gain,
                          std::array<float, 65>* level_adjustment_gains,
                          float* high_bands_denoising_level_adjustment_gain) {
  for (size_t ch = 0; ch < (*x)[0].size(); ++ch) {
    float power = 0.f;
    for (size_t k = 0; k < (*x)[0][ch].size(); ++k) {
      power += (*x)[0][ch][k] * (*x)[0][ch][k];
    }
    average_mic_powers_[ch] += 0.05f * (power - average_mic_powers_[ch]);
  }

  int strongest_ch = 0;
  for (size_t ch = 1; ch < (*x)[0].size(); ++ch) {
    if (average_mic_powers_[ch] > average_mic_powers_[strongest_ch]) {
      strongest_ch = ch;
    }
  }

  num_blocks_with_same_selection_ = strongest_ch == prev_strongest_channel_
                                        ? num_blocks_with_same_selection_ + 1
                                        : 0;

  prev_strongest_channel_ = strongest_ch;

  if (num_blocks_with_same_selection_ > 100) {
    selected_channel_ = strongest_ch;
  }

  if (selected_channel_ == -1) {
    float one_by_num_channels = 1.f / (*x)[0].size();
    for (size_t block = 0; block < x->size(); ++block) {
      for (size_t ch = 1; ch < (*x)[block].size(); ++ch) {
        for (size_t k = 0; k < (*x)[block][ch].size(); ++k) {
          (*x)[block][0][k] += (*x)[block][ch][k];
        }
      }
      for (size_t k = 0; k < (*x)[block][0].size(); ++k) {
        (*x)[block][0][k] *= one_by_num_channels;
      }
    }
  } else if (selected_channel_ != 0) {
    for (size_t block = 0; block < x->size(); ++block) {
      std::copy((*x)[block][selected_channel_].begin(),
                (*x)[block][selected_channel_].end(), (*x)[block][0].begin());
    }
  }
}

MicSelectorFactory::MicSelectorFactory() {}

std::unique_ptr<EchoControlEnhancer> MicSelectorFactory::Create(
    int sample_rate_hz,
    int num_input_channels) {
  return std::make_unique<MicSelector>(num_input_channels);
}

}  // namespace webrtc
