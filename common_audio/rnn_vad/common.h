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

namespace webrtc {
namespace rnn_vad {

constexpr double kPi = 3.14159265358979323846;

// Sub-band frequency boundaries.
constexpr size_t kNumBands = 22;
constexpr int kBandFrequencyBoundaries[kNumBands] = {
    0,    200,  400,  600,  800,  1000, 1200, 1400, 1600,  2000,  2400,
    2800, 3200, 4000, 4800, 5600, 6800, 8000, 9600, 12000, 15600, 20000};

// TODO(alessiob): Do not change the sample rate until we retrain RNNoise.
constexpr size_t kSampleRate24kHz = 24000;
constexpr size_t kFrameSize10ms24kHz = 240;
constexpr size_t kFrameSize20ms24kHz = 480;

// Pitch analysis params.
constexpr size_t kPitchMinPeriod24kHz = 30;   // 0.00125 s (800 Hz).
constexpr size_t kPitchMaxPeriod24kHz = 384;  // 0.016 s (62.5 Hz).
constexpr size_t kBufSize24kHz = kPitchMaxPeriod24kHz + kFrameSize20ms24kHz;
static_assert(kBufSize24kHz % 2 == 0, "Invalid buffer size.");

// Define a higher minimum pitch period for the initial search. This is used to
// avoid searching for very short periods, for which a refinement step is
// responsible.
constexpr size_t kPitchMinPeriod24kHzPitchSearch = 3 * kPitchMinPeriod24kHz;
static_assert(kPitchMinPeriod24kHz < kPitchMinPeriod24kHzPitchSearch, "");
static_assert(kPitchMinPeriod24kHzPitchSearch < kPitchMaxPeriod24kHz, "");

// FFT analysis.
constexpr size_t kFftLenght20ms24kHz = kFrameSize20ms24kHz;
constexpr size_t kFftNumCoeffs20ms24kHz = kFftLenght20ms24kHz / 2 + 1;

// 12 kHz analysis.
constexpr size_t kSampleRate12kHz = 12000;
constexpr size_t kFrameSize10ms12kHz = 120;
constexpr size_t kFrameSize20ms12kHz = 240;
constexpr size_t kBufSize12kHz = kBufSize24kHz / 2;
constexpr size_t kPitchMinPeriod12kHzPitchSearch =
    kPitchMinPeriod24kHzPitchSearch / 2;
constexpr size_t kPitchMaxPeriod12kHz = kPitchMaxPeriod24kHz / 2;

// 48 kHz constants.
constexpr size_t kSampleRate48kHz = 48000;
constexpr size_t kFrameSize10ms48kHz = 480;
constexpr size_t kFrameSize20ms48kHz = 960;
constexpr size_t kPitchMinPeriod48kHz = kPitchMinPeriod24kHz * 2;
constexpr size_t kPitchMaxPeriod48kHz = kPitchMaxPeriod24kHz * 2;
constexpr size_t kBufSize48kHz = kBufSize24kHz * 2;
constexpr size_t kFftLenght20ms48kHz = kFrameSize20ms48kHz;
constexpr size_t kFftNumCoeffs20ms48kHz = kFftLenght20ms48kHz / 2 + 1;

// Feature extraction parameters.
constexpr size_t kNumBandCorrCoeffs = 6;
constexpr size_t kNumBandEnergyCoeffDeltas = 6;
static_assert((0 < kNumBandCorrCoeffs) && (kNumBandCorrCoeffs < kNumBands), "");
static_assert((0 < kNumBandEnergyCoeffDeltas) &&
                  (kNumBandEnergyCoeffDeltas < kNumBands),
              "");
constexpr size_t kSpectralCoeffsHistorySize = 8;
static_assert(kSpectralCoeffsHistorySize > 2,
              "The history size must at least be 2 to compute first and second "
              "derivatives.");
}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_COMMON_H_
