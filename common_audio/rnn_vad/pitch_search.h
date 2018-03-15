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

// Compute a gain threshold for a pitch candidate with period |t1| given the
// initial estimate pitch having period |t0| and gain |g0| and the last frame
// estimated pitch having period |t_prev| and gain |g_prev|. |k| is the factor
// used to derive |t1| from |t0|.
float ComputePitchGainThreshold(size_t t1,
                                size_t k,
                                size_t t0,
                                float g0,
                                size_t t_prev,
                                size_t g_prev);

// Compute the sum of squared samples for every sliding frame in the pitch
// buffer. The result is written in |yy_values|, the indexes of which are lags
// - i.e., not inverted lags.
void ComputeSlidingFrameSquareEnergies(
    rtc::ArrayView<const float, kBufSize> pitch_buf,
    rtc::ArrayView<float, kPitchMaxPeriod + 1> yy_values);

// Computes the auto-correlation coefficients for a given pitch interval and
// writes them in |auto_corr|. The indexes follow an inverted order, so the
// first elements correspond to the maximum pitch period and the last to the
// minimum one.
void ComputePitchAutoCorrelation(rtc::ArrayView<const float> pitch_buf,
                                 const size_t max_pitch_period,
                                 rtc::ArrayView<float> auto_corr);

// Given the auto-correlation coefficients stored according to
// ComputePitchAutoCorrelation() - i.e., using inverted order, return the best
// and the second best pitch periods
std::array<size_t, 2> FindBestPitchPeriods(
    rtc::ArrayView<const float> auto_corr,
    rtc::ArrayView<const float> pitch_buf,
    const size_t max_pitch_period);

// Refine the pitch period estimation given the pitch buffer |pitch_buf| and the
// initial pitch period estimation |inv_lags|, which is a set of two periods
// encoded as inverted lags. It applies pseudo-interpolation and therefore
// returns an inverted lag at 2x sample rate.
size_t RefinePitchPeriod(rtc::ArrayView<const float, kBufSize> pitch_buf,
                         rtc::ArrayView<const size_t, 2> inv_lags);

struct PitchInfo {
  PitchInfo() : period(0), gain(0.f) {}
  PitchInfo(size_t p, float g) : period(p), gain(g) {}
  size_t period;
  float gain;
};

// Extend the pitch period search in the interval that has been ignored in the
// initial pitch search (i.e., [|kPitchMinPeriod|,
// |kHalfPitchMinPeriodPitchSearch|]) to find the final pitch period and its
// gain. |x| is the pitch buffer, |pitch_period_2x| is the initial estimation at
// 2x sample rate, and |last_pitch_2x| comes from the previous frame.
// It returns the new pitch estimation (period and gain, with period at 2x
// sample rate).
PitchInfo CheckLowerPitchPeriodsAndComputePitchGain(
    rtc::ArrayView<const float, kBufSize> pitch_buf,
    const size_t pitch_period_2x,
    const PitchInfo last_pitch_2x);

// Search the pitch period in |pitch_buf|.
PitchInfo PitchSearch(rtc::ArrayView<const float, kBufSize> pitch_buf,
                      const PitchInfo last_pitch);

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_PITCH_SEARCH_H_
