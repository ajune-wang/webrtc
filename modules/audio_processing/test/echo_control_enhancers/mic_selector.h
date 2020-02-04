/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TEST_ECHO_CONTROL_ENHANCERS_MIC_SELECTOR_H_
#define MODULES_AUDIO_PROCESSING_TEST_ECHO_CONTROL_ENHANCERS_MIC_SELECTOR_H_

#include <array>
#include <memory>
#include <vector>

#include "api/audio/echo_control_enhancer.h"
#include "rtc_base/checks.h"

namespace webrtc {

class MicSelector : public EchoControlEnhancer {
 public:
  explicit MicSelector(size_t num_input_channels);

  void Process(const std::vector<std::array<float, 65>*>& X0_fft_re,
               const std::vector<std::array<float, 65>*>& X0_fft_im,
               std::vector<std::vector<std::vector<float>>>* x,
               std::array<float, 65>* denoising_gains,
               float* high_bands_denoising_gain,
               std::array<float, 65>* level_adjustment_gains,
               float* high_bands_denoising_level_adjustment_gain) override;

  float AlgorithmicDelayInMs() const override { return 0.f; }
  bool ModifiesInputSignal() const override { return true; }
  size_t NumOutputChannels() const override { return 1; }

  void SetDirection(float x, float y, float z) override {}

 private:
  std::vector<float> average_mic_powers_;
  int selected_channel_ = -1;
  int prev_strongest_channel_ = -1;
  size_t num_blocks_with_same_selection_ = 0;
};

class MicSelectorFactory : public EchoControlEnhancerFactory {
 public:
  MicSelectorFactory();
  std::unique_ptr<EchoControlEnhancer> Create(int sample_rate_hz,
                                              int num_input_channels) override;

 private:
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_ECHO_CONTROL_ENHANCERS_MIC_SELECTOR_H_
