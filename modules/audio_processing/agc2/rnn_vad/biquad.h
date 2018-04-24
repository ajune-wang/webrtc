/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_BIQUAD_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_BIQUAD_H_

#include "api/array_view.h"

namespace webrtc {
namespace rnn_vad {

// Bi-quad filter.
class BiQuadFilter {
 public:
  // Normalized filter coefficients.
  //         1 + a_1 • z^(-1) + a_2 • z^(-2)
  // H(z) = ---------------------------------
  //        b_0 + b_1 • z^(-1) + b_2 • z^(-2)
  struct Config {
    Config() : a1(0.f), a2(0.f), b0(1.f), b1(0.f), b2(0.f) {}
    Config(float a1, float a2, float b0, float b1, float b2)
        : a1(a1), a2(a2), b0(b0), b1(b1), b2(b2) {}
    float a1;
    float a2;
    float b0;
    float b1;
    float b2;
  };

  struct State {
    State() : m0(0.f), m1(0.f) {}
    State(float m0, float m1) : m0(m0), m1(m1) {}
    float m0;
    float m1;
  };

  explicit BiQuadFilter(const Config config);
  BiQuadFilter(const BiQuadFilter&) = delete;
  BiQuadFilter& operator=(const BiQuadFilter&) = delete;
  ~BiQuadFilter();
  void Reset();
  void SetState(const State state);
  // Process a frame. In-place modification is allowed.
  void ProcessFrame(rtc::ArrayView<const float> x, rtc::ArrayView<float> y);

 private:
  const Config config_;
  State state_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_BIQUAD_H_
