/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_BIQUAD_H_
#define COMMON_AUDIO_RNN_VAD_BIQUAD_H_

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
    Config(float a1_, float a2_, float b0_, float b1_, float b2_)
        : a1(a1_), a2(a2_), b0(b0_), b1(b1_), b2(b2_) {}
    float a1;
    float a2;
    float b0;
    float b1;
    float b2;
  };

  struct State {
    State() : m0(0.f), m1(0.f) {}
    State(float m0_, float m1_) : m0(m0_), m1(m1_) {}
    float m0;
    float m1;
  };

  BiQuadFilter();
  explicit BiQuadFilter(const Config config);
  BiQuadFilter(const BiQuadFilter&) = delete;
  BiQuadFilter& operator=(const BiQuadFilter&) = delete;
  ~BiQuadFilter();
  void SetState(const State state);
  // Process a frame. In-place modification is allowed.
  void ProcessFrame(rtc::ArrayView<const float> x, rtc::ArrayView<float> y);

 private:
  const Config config_;
  State state_;
};

// Hard-coded values used for bit exactness unit tests.
// These are not placed in test_utils.h to avoid circular dependency.
const BiQuadFilter::Config kHpfConfig48kHz(-1.99599, 0.99600, 1, -2, 1);
const BiQuadFilter::State kHpfInitialState48kHz(-45.993244171142578125f,
                                                45.930263519287109375f);

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_BIQUAD_H_
