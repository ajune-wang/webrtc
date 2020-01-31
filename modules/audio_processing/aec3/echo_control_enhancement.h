/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_CONTROL_ENHANCEMENT_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_CONTROL_ENHANCEMENT_H_

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/audio/echo_control_enhancer.h"
#include "modules/audio_processing/aec3/block_delay_buffer.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "modules/audio_processing/aec3/suppressor_gain_delay_buffer.h"

namespace webrtc {

// Performs echo control enhancement using a provided custom echo control
// enhancement module. Does not take ownership of the echo control enhancement
// module.
class EchoControlEnhancement {
 public:
  EchoControlEnhancement(size_t num_capture_channels,
                         size_t num_bands,
                         EchoControlEnhancer* enhancer);
  ~EchoControlEnhancement();
  EchoControlEnhancement(const EchoControlEnhancement&) = delete;
  EchoControlEnhancement& operator=(const EchoControlEnhancement&) = delete;

  bool ModifiesOutput() const {
    return enhancer_->ModifiesInputSignal() ||
           enhancer_->AlgorithmicDelayInMs() != 0.f;
  }

  size_t NumOutputChannels() const { return enhancer_->NumOutputChannels(); }

  void Enhance(
      bool use_linear_filter_output,
      rtc::ArrayView<const std::array<float, kFftLengthBy2>>
          linear_filter_output,
      std::vector<std::vector<std::vector<float>>>* y,
      rtc::ArrayView<FftData> Y,
      rtc::ArrayView<float, kFftLengthBy2Plus1>
          low_band_noise_suppression_gains,
      float* high_bands_noise_suppression_gain,
      rtc::ArrayView<float, kFftLengthBy2Plus1> low_band_echo_suppression_gains,
      float* high_bands_echo_suppression_gain);

 private:
  size_t num_capture_channels_;
  std::vector<std::array<float, kFftLengthBy2Plus1>*> Y_re_;
  std::vector<std::array<float, kFftLengthBy2Plus1>*> Y_im_;
  EchoControlEnhancer* const enhancer_;
  std::unique_ptr<SuppressorGainDelayBuffer> gain_delay_buffer_;
  std::unique_ptr<BlockDelayBuffer> signal_delay_buffer_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ECHO_CONTROL_ENHANCEMENT_H_
