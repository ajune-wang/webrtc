/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_PITCH_SEARCH_INTERNAL_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_PITCH_SEARCH_INTERNAL_H_

#include <stddef.h>

#include <array>
#include <utility>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/pitch_info.h"

namespace webrtc {
namespace rnn_vad {

// Performs 2x decimation without any anti-aliasing filter.
void Decimate2x(rtc::ArrayView<const float, kBufSize24kHz> src,
                rtc::ArrayView<float, kBufSize12kHz> dst);

// Computes the sum of squared samples for every sliding frame in the pitch
// buffer. |yy_values| indexes are lags.
//
// The pitch buffer is structured as depicted below:
// |.........|...........|
//      a          b
// The part on the left, named "a" contains the oldest samples, whereas "b" the
// most recent ones. The size of "a" corresponds to the maximum pitch period,
// that of "b" to the frame size (e.g., 16 ms and 20 ms respectively).
void ComputeSlidingFrameSquareEnergies(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer,
    rtc::ArrayView<float, kMaxPitch24kHz + 1> yy_values);

// Top-2 pitch period candidates.
struct CandidatePitchPeriods {
  int best;
  int second_best;
};

// Computes the candidate pitch periods given the auto-correlation coefficients
// stored according to ComputePitchAutoCorrelation() (i.e., using inverted
// lags). The return periods are inverted lags.
CandidatePitchPeriods FindBestPitchPeriods12kHz(
    rtc::ArrayView<const float, kNumInvertedLags12kHz> auto_correlation,
    rtc::ArrayView<const float, kBufSize12kHz> pitch_buffer);

// Refines the pitch period estimation given the pitch buffer |pitch_buffer|,
// the energies for the sliding frames |y_energy| and the initial pitch period
// estimation |pitch_candidates| (inverted lags). Returns an inverted lag at
// 48 kHz.
int RefinePitchPeriod48kHz(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer,
    rtc::ArrayView<const float, kMaxPitch24kHz + 1> y_energy,
    CandidatePitchPeriods pitch_candidates);

// Refines the pitch period estimation and compute the pitch gain. Returns the
// refined pitch estimation data at 48 kHz.
PitchInfo CheckLowerPitchPeriodsAndComputePitchGain(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer,
    rtc::ArrayView<const float, kMaxPitch24kHz + 1> y_energy,
    int initial_pitch_period_48kHz,
    PitchInfo last_pitch_48kHz);

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_PITCH_SEARCH_INTERNAL_H_
