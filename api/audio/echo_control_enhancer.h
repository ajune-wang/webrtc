/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_ECHO_CONTROL_ENHANCER_H_
#define API_AUDIO_ECHO_CONTROL_ENHANCER_H_

#include <memory>

#include "rtc_base/checks.h"

namespace webrtc {

class EchoControlEnhancer {
 public:
  // Processes the multi-channel
  virtual void Process(const std::vector<std::array<float, 65>*>& X0_fft_re,
                       const std::vector<std::array<float, 65>*>& X0_fft_im,
                       std::vector<std::vector<std::vector<float>>>* x) = 0;

  // Returns the algorithmic delay in ms for the processing in the module.
  virtual float AlgorithmicDelayInMs() const = 0;

  // Returns whether the algorithm modifies the input signal.
  virtual bool ModifiesInputSignal() const = 0;

  // Return the number of output channels.
  virtual size_t NumOutputChannels() const = 0;

  virtual ~EchoControlEnhancer() = default;
};

}  // namespace webrtc

#endif  // API_AUDIO_ECHO_CONTROL_ENHANCER_H_
