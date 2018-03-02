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
#include <numeric>
#include <utility>

#include "common_audio/rnn_vad/common.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

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
  size_t inverted_lag = 0;
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
size_t PitchPseudoInterpolation(
    rtc::ArrayView<float> auto_corr, size_t inv_lag) {
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
  return 2 * inv_lag  + offset;
}

// float ComputePitchGain(float xy, float xx, float yy) {
//   return xy / sqrt(1 + xx * yy);
// }

}  // namespace

void ComputePitchAutoCorrelation(rtc::ArrayView<float> auto_corr,
                                 rtc::ArrayView<const float> pitch_buf,
                                 const size_t max_pitch_period) {
  const size_t min_pitch_period = max_pitch_period - auto_corr.size();
  RTC_DCHECK_LT(min_pitch_period, max_pitch_period);
  RTC_DCHECK_LT(max_pitch_period, pitch_buf.size());
  const size_t frame_size = pitch_buf.size() - max_pitch_period;
  // Fixed audio frame compared to the lagged audio frames.
  const rtc::ArrayView<const float> x(pitch_buf.data() + max_pitch_period,
                                      frame_size);
  // Compute auto-correlation coefficients.
  // TODO(alessiob): Optimize using vectorization.
  for (size_t inverted_lag = 0; inverted_lag < auto_corr.size(); ++inverted_lag)
    auto_corr[inverted_lag] = std::inner_product(
        x.begin(), x.end(), pitch_buf.begin() + inverted_lag, 0.f);
}

std::array<size_t, 2> FindBestPitchPeriods(
    rtc::ArrayView<const float> auto_corr,
    rtc::ArrayView<const float> pitch_buf,
    const size_t max_pitch_period) {
  const size_t min_pitch_period = max_pitch_period - auto_corr.size();
  RTC_DCHECK_LT(min_pitch_period, max_pitch_period);
  RTC_DCHECK_LT(max_pitch_period, pitch_buf.size());
  const size_t frame_size = pitch_buf.size() - max_pitch_period;
  // Init |frame_squared_energy| knowing how ComputePitchAutoCorrelation()
  // computed the cross-correlation coefficient for |min_pitch_period|.
  float frame_squared_energy =
      std::inner_product(pitch_buf.begin(), pitch_buf.begin() + frame_size,
                         pitch_buf.begin(), 0.f);
  // Search best and second best pitches by looking at the scaled
  // auto-correlation.
  PitchCandidate candidate, best, second_best;
  for (size_t inverted_lag = 0; inverted_lag < auto_corr.size();
       ++inverted_lag) {
    // A pitch candidate must have positive correlation.
    if (auto_corr[inverted_lag] > 0) {
      candidate.inverted_lag = inverted_lag;
      candidate.num = auto_corr[inverted_lag] * auto_corr[inverted_lag];
      candidate.den = frame_squared_energy;
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
    const float new_coeff = pitch_buf[inverted_lag];
    const float old_coeff = pitch_buf[inverted_lag + frame_size];
    frame_squared_energy += new_coeff * new_coeff - old_coeff * old_coeff;
  }
  return {best.inverted_lag, second_best.inverted_lag};
}

size_t RefinePitchPeriod(rtc::ArrayView<const float, kBufSize> pitch_buf,
                         rtc::ArrayView<const size_t, 2> inv_lags) {
  // Compute the auto-correlation terms only for neighbors of the given pitch
  // candidates (similar to what is done in ComputePitchAutoCorrelation(), but
  // for a few lag values).
  std::array<float, kPitchMaxPeriod - kPitchMinPeriodPitchSearch> auto_corr;
  auto_corr.fill(0.f);  // A zero tells FindBestPitchPeriods() to ignore a lag.
  // Fixed audio frame compared to the lagged audio frames.
  const rtc::ArrayView<const float> x(pitch_buf.data() + kPitchMaxPeriod,
                                      pitch_buf.size() - kPitchMaxPeriod);
  auto is_neighbor = [](int i, int j) { return abs(i - j) <= 2; };
  for (size_t inv_lag = 0; inv_lag < auto_corr.size(); ++inv_lag) {
    // TODO(alessiob): Maybe optimize inner product using vectorization.
    if (is_neighbor(inv_lag, inv_lags[0]) || is_neighbor(inv_lag, inv_lags[1]))
      auto_corr[inv_lag] = std::max(-1.f, std::inner_product(
          x.begin(), x.end(), pitch_buf.begin() + inv_lag, 0.f));
  }
  // Find best pitch in full band.
  const auto pitch_candidates_inverted_lag = FindBestPitchPeriods(
      {auto_corr}, {pitch_buf}, kPitchMaxPeriod);
  const auto inv_lag = pitch_candidates_inverted_lag[0];  // Refine the best.
  // Pseudo-interpolation.
  return PitchPseudoInterpolation({auto_corr}, inv_lag);
}

PitchInfo CheckLowerPitchPeriodsAndComputePitchGain(
    rtc::ArrayView<const float, kBufSize> x,
    const size_t pitch_period_2x,
    const PitchInfo last_pitch) {
  // TODO(alessiob): Implement.
  return {10, 1.f};
}

PitchInfo PitchSearch(rtc::ArrayView<const float, kBufSize> x,
                      const PitchInfo last_pitch) {
  // Decimate 2x (with no anti-aliasing filter) to perform the initial pitch
  // search in half-band.
  std::array<float, kHalfBufSize> x_decimated;
  for (size_t i = 0; i < x_decimated.size(); ++i)
    x_decimated[i] = x[2 * i];
  // Compute auto-correlation terms for the pitch interval
  // [|kHalfPitchMinPeriodPitchSearch|, |kHalfPitchMaxPeriod|]. The indexes of
  // |auto_corr| are inverted lag values.
  std::array<float, kHalfPitchMaxPeriod - kHalfPitchMinPeriodPitchSearch>
      auto_corr;
  ComputePitchAutoCorrelation({auto_corr}, {x_decimated}, kHalfPitchMaxPeriod);
  // Search pitch in half-band.
  auto pitch_candidates_inverted_lag =
      FindBestPitchPeriods({auto_corr}, {x_decimated}, kHalfPitchMaxPeriod);
  // Refine the pitch period estimation in full-band.
  for (auto& inverted_lag : pitch_candidates_inverted_lag)
    inverted_lag *= 2;  // Account for 2x decimation.
  const auto pitch_period =
      kHalfPitchMaxPeriod  // Account for inverted lag.
      - RefinePitchPeriod(x, {pitch_candidates_inverted_lag.data(),
                              pitch_candidates_inverted_lag.size()});
  // Look for stronger harmonics to find the final pitch period and its gain.
  return CheckLowerPitchPeriodsAndComputePitchGain(x, pitch_period, last_pitch);
}

}  // namespace rnn_vad
}  // namespace webrtc
