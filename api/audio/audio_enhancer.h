/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_AUDIO_ENHANCER_H_
#define API_AUDIO_AUDIO_ENHANCER_H_

#include <memory>
#include <vector>

#include "api/scoped_refptr.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

// Interface for an audio enhancer module, defining the necessary functionality
// for allowing it to be injected into APM for inclusion into echo control
// objects.
class AudioEnhancer : public rtc::RefCountInterface {
 public:
  // Processes the audio
  virtual void Process(const std::vector<std::array<float, 65>*>& X0_fft_re,
                       const std::vector<std::array<float, 65>*>& X0_fft_im,
                       std::vector<std::vector<std::vector<float>>>* x,
                       std::array<float, 65>* denoising_gains,
                       float* high_bands_denoising_gain,
                       std::array<float, 65>* level_adjustment_gains,
                       float* high_bands_level_adjustment_gain) = 0;

  // Returns the algorithmic delay in ms for the processing in the module.
  virtual float AlgorithmicDelayInMs() const = 0;

  // Returns whether the algorithm modifies the input signal.
  virtual bool ModifiesInputSignal() const = 0;

  // Return the number of output channels.
  virtual size_t NumOutputChannels() const = 0;
};

// Interface for a factory that creates AudioEnhancers.
class AudioEnhancerController {
 public:
  virtual rtc::scoped_refptr<AudioEnhancer> Create(int sample_rate_hz,
                                                   int num_input_channels) = 0;

  // Updates the properties for the created enhancer if needed.
  virtual void UpdateEnhancementProperties() = 0;

  virtual ~AudioEnhancerController() {}
};

}  // namespace webrtc

#endif  // API_AUDIO_AUDIO_ENHANCER_H_
