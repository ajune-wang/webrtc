/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <numeric>
#include <type_traits>

#include "modules/audio_processing/agc2/rnn_vad/biquad.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
// TODO(https://bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace test {
namespace {

constexpr size_t kFrameSize = 8;
constexpr size_t kNumFrames = 4;
using FloatArraySequence =
    std::array<std::array<float, kFrameSize>, kNumFrames>;

constexpr FloatArraySequence kBiQuadInputSeq = {
    {{-87.166290f, -8.029022f, 101.619583f, -0.294296f, -5.825764f, -8.890625f,
      10.310432f, 54.845333f},
     {-64.647644f, -6.883945f, 11.059189f, -95.242538f, -108.870834f,
      11.024944f, 63.044102f, -52.709583f},
     {-32.350529f, -18.108028f, -74.022339f, -8.986874f, -1.525581f,
      103.705513f, 6.346226f, -14.319557f},
     {22.645832f, -64.597153f, 55.462521f, -109.393188f, 10.117825f,
      -40.019642f, -98.612228f, -8.330326f}}};

constexpr FloatArraySequence kBiQuadOutputSeq = {
    {{-86.68354497f, -7.02175351f, 102.10290352f, -0.37487333f, -5.87205847f,
      -8.85521608f, 10.33772563f, 54.51157181f},
     {-64.92531604f, -6.76395978f, 11.15534507f, -94.68073341f, -107.18177856f,
      13.24642474f, 64.84288941f, -50.97822629f},
     {-30.1579652f, -15.64850899f, -71.06662821f, -5.5883229f, 1.91175353f,
      106.5572003f, 8.57183046f, -12.06298473f},
     {24.84286614f, -62.18094158f, 57.91488056f, -106.65685933f, 13.38760103f,
      -36.60367134f, -94.44880104f, -3.59920354f}}};

// Generate via "B, A = scipy.signal.iirfilter(2, 30/12000, btype='highpass')".
const BiQuadFilter::Config kBiQuadConfig(-1.98889291f,
                                         0.98895425f,
                                         0.99446179f,
                                         -1.98892358f,
                                         0.99446179f);

}  // namespace

// Compare BiQuadFilter implementation to scipy.signal.lfilter.

TEST(RnnVadTest, BiQuadFilterNotInPlace) {
  BiQuadFilter filter(kBiQuadConfig);
  std::array<float, kFrameSize> samples;

  // TODO(https://bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;

  for (size_t i = 0; i < kNumFrames; ++i) {
    filter.ProcessFrame({kBiQuadInputSeq[i]}, {samples});
    ExpectNearRelative({kBiQuadOutputSeq[i]}, {samples}, 1e-4f);
  }
}

TEST(RnnVadTest, BiQuadFilterInPlace) {
  BiQuadFilter filter(kBiQuadConfig);
  std::array<float, kFrameSize> samples;

  // TODO(https://bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;

  for (size_t i = 0; i < kNumFrames; ++i) {
    std::copy(kBiQuadInputSeq[i].begin(), kBiQuadInputSeq[i].end(),
              samples.begin());
    filter.ProcessFrame({samples}, {samples});
    ExpectNearRelative({kBiQuadOutputSeq[i]}, {samples}, 1e-4f);
  }
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
