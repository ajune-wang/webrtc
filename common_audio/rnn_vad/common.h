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

#include <cmath>

namespace webrtc {
namespace rnn_vad {

const double kPi = std::acos(-1);

// Opus frequency band boundaries.
constexpr size_t kNumOpusBands = 22;
constexpr int kOpusBandsFrequencies[kNumOpusBands] = {
    0,    200,  400,  600,  800,  1000, 1200, 1400, 1600,  2000,  2400,
    2800, 3200, 4000, 4800, 5600, 6800, 8000, 9600, 12000, 15600, 20000};

// Do not change the sample rate until we retrain RNNoise.
constexpr size_t kSampleRate = 24000;
constexpr size_t k10msFrameSize = 240;
constexpr size_t k20msFrameSize = 2 * k10msFrameSize;

// Pitch analysis params.
constexpr size_t kPitchMinPeriod = 30;   // 0.00125 s (800 Hz).
constexpr size_t kPitchMaxPeriod = 384;  // 0.016 s (62.5 Hz).
constexpr size_t kBufSize = kPitchMaxPeriod + k20msFrameSize;
static_assert(kBufSize % 2 == 0, "Invalid full band buffer size.");

// Define a higher minimum pitch period for the initial search. This is used to
// avoid searching for very short periods, for which a refinement step is
// responsible.
constexpr size_t kPitchMinPeriodPitchSearch = 3 * kPitchMinPeriod;
static_assert(kPitchMinPeriod < kPitchMinPeriodPitchSearch, "");
static_assert(kPitchMinPeriodPitchSearch < kPitchMaxPeriod, "");

// Half-band analysis.
constexpr size_t kHalfSampleRate = kSampleRate / 2;
constexpr size_t kHalf10msFrameSize = k10msFrameSize / 2;
constexpr size_t kHalf20msFrameSize = k20msFrameSize / 2;
constexpr size_t kHalfBufSize = kBufSize / 2;
constexpr size_t kHalfPitchMinPeriodPitchSearch =
    kPitchMinPeriodPitchSearch / 2;
constexpr size_t kHalfPitchMaxPeriod = kPitchMaxPeriod / 2;

// FFT analysis in half-band.
constexpr size_t kHalf20msFftLenght = 256;  // Next ^2 of |kHalf20msFrameSize|.
static_assert(kHalf20msFftLenght / 2 < kHalf20msFrameSize &&
                  kHalf20msFrameSize <= kHalf20msFftLenght,
              "FFT size and frame size are not compatible.");
static_assert((kHalf20msFftLenght & (kHalf20msFftLenght - 1)) == 0,
              "The FFT size must be a power of 2.");
constexpr size_t kHalf20msNumFftPoints = kHalf20msFftLenght / 2 + 1;

// Double-band constants.
constexpr size_t kPitchMinPeriod2x = 2 * kPitchMinPeriod;
constexpr size_t kPitchMaxPeriod2x = 2 * kPitchMaxPeriod;

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_COMMON_H_
