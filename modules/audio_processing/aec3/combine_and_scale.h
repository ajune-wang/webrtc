/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_COMBINE_AND_SCALE_H_
#define MODULES_AUDIO_PROCESSING_AEC3_COMBINE_AND_SCALE_H_

#include <array>

#include "api/array_view.h"
#include "api/audio/echo_control_enhancer.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "modules/audio_processing/utility/ooura_fft.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

class CombineAndScale : public EchoControlEnhancer {
 public:
  CombineAndScale(size_t num_output_channels,
                  float algorithmic_delay,
                  bool modifies_input_signal);

  void Process(const std::vector<std::array<float, 65>*>& X0_fft_re,
               const std::vector<std::array<float, 65>*>& X0_fft_im,
               std::vector<std::vector<std::vector<float>>>* x) override;

  float AlgorithmicDelayInMs() const override { return algorithmic_delay_; }
  bool ModifiesInputSignal() const override { return modifies_input_signal_; }
  size_t NumOutputChannels() const override { return num_output_channels_; }

  void SetDirection(float x, float y, float z) override {
    x_ = x;
    y_ = y;
    z_ = z;
  }

 private:
  const size_t num_input_channels_;
  const size_t num_output_channels_;
  const float algorithmic_delay_;
  const bool modifies_input_signal_;
  float x_ = 0;
  float y_ = 0;
  float z_ = 0;
};

class CombineAndScaleFactory : public EchoControlEnhancerFactory {
 public:
  CombineAndScaleFactory(float algorithmic_delay, bool modifies_input_signal);
  std::unique_ptr<EchoControlEnhancer> Create(int sample_rate_hz,
                                              int num_input_channels) override;

 private:
  const float algorithmic_delay_;
  const bool modifies_input_signal_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_COMBINE_AND_SCALE_H_
