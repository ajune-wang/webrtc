/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_COMMON_H_
#define COMMON_AUDIO_RNN_VAD_COMMON_H_

// TODO(alessiob): Remove once debugged.
#define TEST_AT_48K 1

#include "api/array_view.h"

namespace webrtc {
namespace rnn_vad {

// Opus frequency band boundaries.
constexpr size_t kNumOpusBands = 22;
const int kOpusBandsFrequencies[kNumOpusBands] = {
    0,    200,  400,  600,  800,  1000, 1200, 1400, 1600,  2000,  2400,
    2800, 3200, 4000, 4800, 5600, 6800, 8000, 9600, 12000, 15600, 20000};

#ifdef TEST_AT_48K
constexpr size_t kSampleRate = 48000;
constexpr size_t kInputFrameSize = 480;
#else
// Do not change the sample rate until we retrain RNNoise.
constexpr size_t kSampleRate = 24000;
constexpr size_t kInputFrameSize = 256;
#endif
constexpr size_t kFrameSize = 2 * kInputFrameSize;  // Sliding win 50% overlap.

// Pitch analysis params.
#ifdef TEST_AT_48K
constexpr size_t kPitchMinPeriod = 60;   // 0.00125 s (800 Hz).
constexpr size_t kPitchMaxPeriod = 768;  // 0.016 s (62.5 Hz).
#else
constexpr size_t kPitchMinPeriod = 30;   // 0.00125 s (800 Hz).
constexpr size_t kPitchMaxPeriod = 384;  // 0.016 s (62.5 Hz).
#endif
constexpr size_t kBufSize = kPitchMaxPeriod + kFrameSize;
static_assert(kBufSize % 2 == 0, "Invalid full band buffer size.");

// Half-band analysis.
constexpr size_t kHalfSampleRate = kSampleRate / 2;
constexpr size_t kHalfInputFrameSize = kInputFrameSize / 2;
constexpr size_t kHalfFrameSize = kFrameSize / 2;
constexpr size_t kHalfBufSize = kBufSize / 2;

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_COMMON_H_
