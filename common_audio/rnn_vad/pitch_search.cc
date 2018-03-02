/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/pitch_search.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

#include "common_audio/rnn_vad/common.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// Notes on pitch buffer and auto-correlation.
//
// The pitch buffer is structured as depicted below:
// |.........|...........|
//      a          b
// The part on the left, named "a" contains the oldest samples, whereas "b"
// most recent ones. The size of "a" corresponds to the maximum pitch period,
// that of "b" to the frame size (e.g., 16 ms and 20 ms respectively).
//
// In this file, the auto-correlations coefficients are computed as follows:
// |.........|...........|
//           [ x (fixed) ]
// [   y_0   ]
//         [ y_{m-1} ]
// x and y are sub-array of equal length; x is never moved, whereas y is
// sliding. The cross-correlation between y_0 and x corresponds to the
// auto-correlation for the maximum pitch period.
//
// This solution leads to cleaner code with the only need to introduce "inverted
// lags", which are auto-correlation indexes in the range
// {0, max pitch period - min pitch period}. Hence, an inverted lag equal to 0
// corresponds to the maximum pitch period, while the last index to the
// minimum one.
//
// Finally, variable names are named as follows:
// - pitch buffer: pitch_buf;
// - x and y frames for auto-correlation: x, y;
// - xx/yy is the auto-correlation with no lag for x/y;
// - xy is the cross-correlation between x and y;
// - inverted lag indexes: inv_lag(s).

// Define a higher minimum pitch period for the initial search. This is used to
// avoid searching for very short periods, for which a refinement step is
// responsible.
constexpr size_t kPitchMinPeriodPitchSearch = 3 * kPitchMinPeriod;
static_assert(kPitchMinPeriod < kPitchMinPeriodPitchSearch, "");
static_assert(kPitchMinPeriodPitchSearch < kPitchMaxPeriod, "");

// Half-band constants.
constexpr size_t kHalfPitchMinPeriodPitchSearch =
    kPitchMinPeriodPitchSearch / 2;
constexpr size_t kHalfPitchMaxPeriod = kPitchMaxPeriod / 2;

struct PitchCandidate {
  size_t inv_lag = 0;
  // The |num|/|den| ratio is used to rank pitch candidates.
  float num = -1.f;  // Numerator.
  float den = 0.f;   // Denominator.
  bool operator>(const PitchCandidate& b) const {
    // Comparing the ratios without using divisions.
    return num * b.den > b.num * den;
  }
};

// Returns an inverse lag corresponding to |inv_lag| with 2x sample rate and
// applying pseudo-interpolation.
size_t PitchPseudoInterpolation(rtc::ArrayView<float> auto_corr,
                                size_t inv_lag) {
  RTC_DCHECK_LE(0, inv_lag);
  RTC_DCHECK_LT(inv_lag, auto_corr.size());
  int offset = 0;
  // Cannot apply pseudo-interpolation at the boundaries.
  if (inv_lag > 0 && inv_lag < auto_corr.size() - 1) {
    const float a = auto_corr[inv_lag - 1];
    const float b = auto_corr[inv_lag];
    const float c = auto_corr[inv_lag + 1];
    if ((c - a) > 0.7f * (b - a))
      offset = 1;
    else if ((a - c) > 0.7f * (b - c))
      offset = -1;
  }
  return 2 * inv_lag + offset;
}

// Integer multipliers used in CheckLowerPitchPeriodsAndComputePitchGain() when
// looking for sub-harmonics.
// About the values.
// Given the initial pitch period T, we look at shorter periods (or its
// harmonics) by considering T/k with k in {2, ..., 15}. When for example k = 4,
// we should also expect a peak at T*3/4. When k = 8 instead we don't want to
// look at T*2/8, since we have already checked T/4 before. Instead, we look at
// T*3/8.
// The values are hand-tuned in order to look at peaks that we would not expect
// for a different pitch.
const std::array<int, 16> kSubHarmonicMultipliers = {
    0, 0, 0,  // The first three are never used.
    2, 3, 2, 5, 2, 3, 2, 3, 2, 5, 2, 3, 2};

float ComputePitchGain(float xy, float xx, float yy) {
  // TODO(alessiob): Maybe approximate sqrt().
  return xy / std::sqrt(1.f + xx * yy);
}

// TODO(alessiob): Check if there are better interfaces as an inline function.
inline float ComputeAutoCorrelationCoeff(rtc::ArrayView<const float> pitch_buf,
                                         const size_t inv_lag,
                                         const size_t max_pitch_period) {
  // TODO(alessiob): Optimize using vectorization.
  return std::inner_product(pitch_buf.begin() + max_pitch_period,
                            pitch_buf.end(), pitch_buf.begin() + inv_lag, 0.f);
}

}  // namespace

void ComputeSlidingFrameSquareEnergies(
    rtc::ArrayView<float, kPitchMaxPeriod + 1> yy_values,
    rtc::ArrayView<const float, kBufSize> pitch_buf) {
  float yy =
      ComputeAutoCorrelationCoeff(pitch_buf, kPitchMaxPeriod, kPitchMaxPeriod);
  for (size_t i = 1; i < yy_values.size(); ++i) {
    const float old_coeff = pitch_buf[kPitchMaxPeriod + kFrameSize - i];
    const float new_coeff = pitch_buf[kPitchMaxPeriod - i];
    yy -= old_coeff * old_coeff;
    yy += new_coeff * new_coeff;
    yy_values[i] = std::max(0.f, yy);
  }
}

void ComputePitchAutoCorrelation(rtc::ArrayView<float> auto_corr,
                                 rtc::ArrayView<const float> pitch_buf,
                                 const size_t max_pitch_period) {
  const size_t min_pitch_period = max_pitch_period - auto_corr.size();
  RTC_DCHECK_LT(min_pitch_period, max_pitch_period);
  RTC_DCHECK_LT(max_pitch_period, pitch_buf.size());
  // Compute auto-correlation coefficients.
  for (size_t inv_lag = 0; inv_lag < auto_corr.size(); ++inv_lag)
    auto_corr[inv_lag] =
        ComputeAutoCorrelationCoeff(pitch_buf, inv_lag, max_pitch_period);
}

std::array<size_t, 2> FindBestPitchPeriods(
    rtc::ArrayView<const float> auto_corr,
    rtc::ArrayView<const float> pitch_buf,
    const size_t max_pitch_period) {
  const size_t min_pitch_period = max_pitch_period - auto_corr.size();
  RTC_DCHECK_LT(min_pitch_period, max_pitch_period);
  RTC_DCHECK_LT(max_pitch_period, pitch_buf.size());
  const size_t frame_size = pitch_buf.size() - max_pitch_period;
  float yy = ComputeAutoCorrelationCoeff(pitch_buf, 0, max_pitch_period);
  // Search best and second best pitches by looking at the scaled
  // auto-correlation.
  PitchCandidate candidate, best, second_best;
  for (size_t inv_lag = 0; inv_lag < auto_corr.size(); ++inv_lag) {
    // A pitch candidate must have positive correlation.
    if (auto_corr[inv_lag] > 0) {
      candidate.inv_lag = inv_lag;
      candidate.num = auto_corr[inv_lag] * auto_corr[inv_lag];
      candidate.den = yy;
      if (candidate > second_best) {
        if (candidate > best) {
          second_best = best;
          best = candidate;
        } else {
          second_best = candidate;
        }
      }
    }
    // Update |squared_energy_y| for the next inverted lag.
    const float new_coeff = pitch_buf[inv_lag];
    const float old_coeff = pitch_buf[inv_lag + frame_size];
    yy -= old_coeff * old_coeff;
    yy += new_coeff * new_coeff;
  }
  return {best.inv_lag, second_best.inv_lag};
}

size_t RefinePitchPeriod(rtc::ArrayView<const float, kBufSize> pitch_buf,
                         rtc::ArrayView<const size_t, 2> inv_lags) {
  // Compute the auto-correlation terms only for neighbors of the given pitch
  // candidates (similar to what is done in ComputePitchAutoCorrelation(), but
  // for a few lag values).
  std::array<float, kPitchMaxPeriod - kPitchMinPeriodPitchSearch> auto_corr;
  auto_corr.fill(0.f);  // Zeros are ignored lags in FindBestPitchPeriods().
  auto is_neighbor = [](int i, int j) { return abs(i - j) <= 2; };
  for (size_t inv_lag = 0; inv_lag < auto_corr.size(); ++inv_lag) {
    if (is_neighbor(inv_lag, inv_lags[0]) || is_neighbor(inv_lag, inv_lags[1]))
      auto_corr[inv_lag] = std::max(
          -1.f,
          ComputeAutoCorrelationCoeff(pitch_buf, inv_lag, kPitchMaxPeriod));
  }
  // Find best pitch in full band.
  const auto pitch_candidates_inv_lags =
      FindBestPitchPeriods({auto_corr}, {pitch_buf}, kPitchMaxPeriod);
  const auto inv_lag = pitch_candidates_inv_lags[0];  // Refine the best.
  // Pseudo-interpolation.
  return PitchPseudoInterpolation({auto_corr}, inv_lag);
}

PitchInfo CheckLowerPitchPeriodsAndComputePitchGain(
    rtc::ArrayView<const float, kBufSize> pitch_buf,
    const size_t pitch_period_2x,
    const PitchInfo last_pitch_2x) {
  // Init.
  const size_t t0 = std::min(pitch_period_2x / 2, kPitchMaxPeriod - 1);
  // const size_t t0_prev = last_pitch_2x.period / 2;
  std::array<float, kPitchMaxPeriod + 1> yy_values;
  ComputeSlidingFrameSquareEnergies({yy_values.data(), yy_values.size()},
                                    {pitch_buf});
  const float xx = yy_values[0];

  // Compute the gain for the given pitch period.
  float xy = ComputeAutoCorrelationCoeff(pitch_buf, kPitchMaxPeriod - 1 - t0,
                                         kPitchMaxPeriod);
  const float g0 = ComputePitchGain(xy, xx, yy_values[t0]);

  // TODO(alessiob): Finalize.
  return {t0 + kSubHarmonicMultipliers[0], g0};
}

PitchInfo PitchSearch(rtc::ArrayView<const float, kBufSize> pitch_buf,
                      const PitchInfo last_pitch) {
  // Decimate 2x (with no anti-aliasing filter) to perform the initial pitch
  // search in half-band.
  std::array<float, kHalfBufSize> pitch_buf_decimated;
  for (size_t i = 0; i < pitch_buf_decimated.size(); ++i)
    pitch_buf_decimated[i] = pitch_buf[2 * i];
  // Compute auto-correlation terms for the pitch interval
  // [|kHalfPitchMinPeriodPitchSearch|, |kHalfPitchMaxPeriod|]. The indexes of
  // |auto_corr| are inverted lag values.
  std::array<float, kHalfPitchMaxPeriod - kHalfPitchMinPeriodPitchSearch>
      auto_corr;
  ComputePitchAutoCorrelation({auto_corr}, {pitch_buf_decimated},
                              kHalfPitchMaxPeriod);
  // Search pitch in half-band.
  auto pitch_candidates_inv_lags = FindBestPitchPeriods(
      {auto_corr}, {pitch_buf_decimated}, kHalfPitchMaxPeriod);
  // Refine the pitch period estimation in full-band.
  for (auto& inv_lag : pitch_candidates_inv_lags)
    inv_lag *= 2;  // Account for 2x decimation.
  const auto pitch_period =
      kHalfPitchMaxPeriod  // Account for inverted lag.
      - RefinePitchPeriod(pitch_buf, {pitch_candidates_inv_lags.data(),
                                      pitch_candidates_inv_lags.size()});
  // Look for stronger harmonics to find the final pitch period and its gain.
  return CheckLowerPitchPeriodsAndComputePitchGain(pitch_buf, pitch_period,
                                                   last_pitch);
}

}  // namespace rnn_vad
}  // namespace webrtc
