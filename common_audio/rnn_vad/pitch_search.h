/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_PITCH_SEARCH_H_
#define COMMON_AUDIO_RNN_VAD_PITCH_SEARCH_H_

#include <array>

#include "api/array_view.h"
#include "common_audio/rnn_vad/common.h"

namespace webrtc {
namespace rnn_vad {

// 2x decimation without any anti-aliasing filter.
void Decimate2xNoAntiAliasignFilter(rtc::ArrayView<const float> src,
                                    rtc::ArrayView<float> dst);

// Compute a gain threshold for a pitch candidate with period |t1| given the
// initial and previous estimates (|t0|, |g0|) and (|t_prev|, |g_prev|). |k| is
// the ratio with which |t1| has been derived from |t0|.
float ComputePitchGainThreshold(size_t t1,
                                size_t k,
                                size_t t0,
                                float g0,
                                size_t t_prev,
                                size_t g_prev);

// Compute the sum of squared samples for every sliding frame in the pitch
// buffer. |yy_values| indexes are lags.
//
// The pitch buffer is structured as depicted below:
// |.........|...........|
//      a          b
// The part on the left, named "a" contains the oldest samples, whereas "b" the
// most recent ones. The size of "a" corresponds to the maximum pitch period,
// that of "b" to the frame size (e.g., 16 ms and 20 ms respectively).
void ComputeSlidingFrameSquareEnergies(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
    rtc::ArrayView<float, kPitchMaxPeriod24kHz + 1> yy_values);

// Compute the auto-correlation coefficients for a given pitch interval.
// |auto_corr| indexes are inverted lags.
//
// The auto-correlations coefficients are computed as follows:
// |.........|...........|  <- pitch buffer
//           [ x (fixed) ]
// [   y_0   ]
//         [ y_{m-1} ]
// x and y are sub-array of equal length; x is never moved, whereas y slides.
// The cross-correlation between y_0 and x corresponds to the auto-correlation
// for the maximum pitch period. Hence, the first value in |auto_corr| has an
// inverted lag equal to 0 that corresponds to a lag equal to the maximum pitch
// period.
void ComputePitchAutoCorrelation(rtc::ArrayView<const float> pitch_buf,
                                 const size_t max_pitch_period,
                                 rtc::ArrayView<float> auto_corr);

// Given the auto-correlation coefficients stored according to
// ComputePitchAutoCorrelation() (i.e., using inverted lags), return the best
// and the second best pitch periods.
std::array<size_t, 2> FindBestPitchPeriods(
    rtc::ArrayView<const float> auto_corr,
    rtc::ArrayView<const float> pitch_buf,
    const size_t max_pitch_period);

// Refine the pitch period estimation given the pitch buffer |pitch_buf| and the
// initial pitch period estimation |inv_lags|. Return an inverted lag at 48 kHz.
size_t RefinePitchPeriod48kHz(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
    rtc::ArrayView<const size_t, 2> inv_lags);

struct PitchInfo {
  PitchInfo() : period(0), gain(0.f) {}
  PitchInfo(size_t p, float g) : period(p), gain(g) {}
  size_t period;
  float gain;
};

// Refine the pitch period estimation and compute the pitch gain. Return the
// refined pitch estimation data at 48 kHz.
PitchInfo CheckLowerPitchPeriodsAndComputePitchGain(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
    const size_t initial_pitch_period_48kHz,
    const PitchInfo prev_pitch_48kHz);

// Search the pitch period and gain. Return the pitch estimation data at 48 kHz.
PitchInfo PitchSearch(rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
                      const PitchInfo prev_pitch_48kHz);

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_PITCH_SEARCH_H_
