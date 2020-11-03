/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/pitch_search_internal.h"

#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>

#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_compare.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// Converts a lag to an inverted lag (only for 24kHz).
constexpr int GetInvertedLag(int lag) {
  RTC_DCHECK_LE(lag, kMaxPitch24kHz);
  return kMaxPitch24kHz - lag;
}

float ComputeAutoCorrelation(
    int inverted_lag,
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer) {
  RTC_DCHECK_LT(inverted_lag, kBufSize24kHz);
  RTC_DCHECK_LE(inverted_lag, kMaxPitch24kHz);
  // TODO(bugs.webrtc.org/9076): Maybe optimize using vectorization.
  return std::inner_product(pitch_buffer.begin() + kMaxPitch24kHz,
                            pitch_buffer.end(),
                            pitch_buffer.begin() + inverted_lag, 0.f);
}

// Given an auto-correlation coefficient `curr` and its neighboring values
// `prev` and `next` computes a pseudo-interpolation offset to be applied to the
// pitch period associated to `curr`. The output is a lag in {-1, 0, +1}.
// TODO(bugs.webrtc.org/9076): Consider removing pseudo-i since it is relevant
// only if the spectral analysis works at a sample rate that is twice as that of
// the pitch buffer (not so important instead for the estimated pitch period
// feature fed into the RNN).
int GetPitchPseudoInterpolationOffset(float prev, float curr, float next) {
  int offset = 0;
  if ((next - prev) > 0.7f * (curr - prev)) {
    offset = 1;  // |next| is the largest auto-correlation coefficient.
  } else if ((prev - next) > 0.7f * (curr - next)) {
    offset = -1;  // |prev| is the largest auto-correlation coefficient.
  }
  return offset;
}

// Refines a pitch period |lag| encoded as lag with pseudo-interpolation. The
// output sample rate is twice as that of |lag|.
int PitchPseudoInterpolationLagPitchBuf(
    int lag,
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer) {
  int offset = 0;
  // Cannot apply pseudo-interpolation at the boundaries.
  if (lag > 0 && lag < kMaxPitch24kHz) {
    offset = GetPitchPseudoInterpolationOffset(
        ComputeAutoCorrelation(GetInvertedLag(lag - 1), pitch_buffer),
        ComputeAutoCorrelation(GetInvertedLag(lag), pitch_buffer),
        ComputeAutoCorrelation(GetInvertedLag(lag + 1), pitch_buffer));
  }
  return 2 * lag + offset;
}

// Refines a pitch period |inverted_lag| encoded as inverted lag with
// pseudo-interpolation. The output sample rate is twice as that of
// |inverted_lag|.
int PitchPseudoInterpolationInvLagAutoCorr(
    int inverted_lag,
    rtc::ArrayView<const float, kNumInvertedLags24kHz> auto_correlation) {
  int offset = 0;
  // Cannot apply pseudo-interpolation at the boundaries.
  if (inverted_lag > 0 && inverted_lag < kNumInvertedLags24kHz - 1) {
    offset = GetPitchPseudoInterpolationOffset(
        auto_correlation[inverted_lag + 1], auto_correlation[inverted_lag],
        auto_correlation[inverted_lag - 1]);
  }
  // TODO(bugs.webrtc.org/9076): When retraining, check if |offset| below should
  // be subtracted since |inverted_lag| is an inverted lag but offset is a lag.
  return 2 * inverted_lag + offset;
}

// Integer multipliers used in CheckLowerPitchPeriodsAndComputePitchGain() when
// looking for sub-harmonics.
// The values have been chosen to serve the following algorithm. Given the
// initial pitch period T, we examine whether one of its harmonics is the true
// fundamental frequency. We consider T/k with k in {2, ..., 15}. For each of
// these harmonics, in addition to the pitch gain of itself, we choose one
// multiple of its pitch period, n*T/k, to validate it (by averaging their pitch
// gains). The multiplier n is chosen so that n*T/k is used only one time over
// all k. When for example k = 4, we should also expect a peak at 3*T/4. When
// k = 8 instead we don't want to look at 2*T/8, since we have already checked
// T/4 before. Instead, we look at T*3/8.
// The array can be generate in Python as follows:
//   from fractions import Fraction
//   # Smallest positive integer not in X.
//   def mex(X):
//     for i in range(1, int(max(X)+2)):
//       if i not in X:
//         return i
//   # Visited multiples of the period.
//   S = {1}
//   for n in range(2, 16):
//     sn = mex({n * i for i in S} | {1})
//     S = S | {Fraction(1, n), Fraction(sn, n)}
//     print(sn, end=', ')
constexpr std::array<int, 14> kSubHarmonicMultipliers = {
    {3, 2, 3, 2, 5, 2, 3, 2, 3, 2, 5, 2, 3, 2}};

// Initial pitch period candidate thresholds for ComputePitchGainThreshold() for
// a sample rate of 24 kHz. Computed as [5*k*k for k in range(16)].
constexpr std::array<int, 14> kInitialPitchPeriodThresholds = {
    {20, 45, 80, 125, 180, 245, 320, 405, 500, 605, 720, 845, 980, 1125}};

// Numeric interval between `first` and `last`.
struct Interval {
  int first;
  int last;
};

// Creates a pitch period interval centered in `inverted_lag` with hard-coded
// radius. Clipping is applied so that the interval is always valid for a 24 kHz
// pitch buffer.
Interval CreateInvertedLagInterval(int inverted_lag) {
  constexpr int kRadius = 2;
  return {std::max(inverted_lag - kRadius, 0),
          std::min(inverted_lag + kRadius, kNumInvertedLags24kHz - 1)};
}

// Computes the auto correlation coefficients for the inverted lags in the
// closed interval `inverted_lags`.
void ComputeAutoCorrelation(
    Interval inverted_lags,
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer,
    rtc::ArrayView<float, kNumInvertedLags24kHz> auto_correlation) {
  for (int inverted_lag = inverted_lags.first;
       inverted_lag <= inverted_lags.last; ++inverted_lag) {
    RTC_DCHECK_GE(inverted_lag, 0);
    RTC_DCHECK_LT(inverted_lag, auto_correlation.size());
    auto_correlation[inverted_lag] =
        ComputeAutoCorrelation(inverted_lag, pitch_buffer);
  }
}

int FindBestPitchPeriods24kHz(
    rtc::ArrayView<const float, kNumInvertedLags24kHz> auto_correlation,
    rtc::ArrayView<const float, kMaxPitch24kHz + 1> y_energy,
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer) {
  static_assert(kMaxPitch24kHz > kNumInvertedLags24kHz, "");
  static_assert(kMaxPitch24kHz < kBufSize24kHz, "");
  int best_inverted_lag = 0;     // Pitch period.
  float best_numerator = -1.f;   // Pitch strength numerator.
  float best_denominator = 0.f;  // Pitch strength denominator.
  for (int inverted_lag = 0; inverted_lag < kNumInvertedLags24kHz;
       ++inverted_lag) {
    // A pitch candidate must have positive correlation.
    if (auto_correlation[inverted_lag] > 0.f) {
      // Auto-correlation energy normalized by frame energy.
      const float numerator =
          auto_correlation[inverted_lag] * auto_correlation[inverted_lag];
      const float denominator = y_energy[kMaxPitch24kHz - inverted_lag];
      // Compare numerator/denominator ratios without using divisions.
      if (numerator * best_denominator > best_numerator * denominator) {
        best_inverted_lag = inverted_lag;
        best_numerator = numerator;
        best_denominator = denominator;
      }
    }
  }
  return best_inverted_lag;
}

}  // namespace

void Decimate2x(rtc::ArrayView<const float, kBufSize24kHz> src,
                rtc::ArrayView<float, kBufSize12kHz> dst) {
  // TODO(bugs.webrtc.org/9076): Consider adding anti-aliasing filter.
  static_assert(2 * dst.size() == src.size(), "");
  for (int i = 0; rtc::SafeLt(i, dst.size()); ++i) {
    dst[i] = src[2 * i];
  }
}

// Computes the pitch gain threshold.
// Arguments:
//      t1: candidate pitch period
//   ratio: pitch period ratio
//      t0: initial pitch period
//      g0: initial pitch gain
//  t_prev: previous pitch period
//  g_prev: previous pitch gain
float ComputePitchGainThreshold(int t1,
                                int ratio,
                                int t0,
                                float g0,
                                int t_prev,
                                float g_prev) {
  RTC_DCHECK_GE(t1, 0);
  RTC_DCHECK_GE(ratio, 2);
  RTC_DCHECK_GE(t0, 0);
  RTC_DCHECK_GE(t_prev, 0);
  // Compute a term that lowers the threshold when |t1| is close to the last
  // estimated period |t_prev| - i.e., pitch tracking.
  float lower_threshold_term = 0;
  if (std::abs(t1 - t_prev) <= 1) {
    // The candidate pitch period is within 1 sample from the previous one.
    // Make the candidate at |t1| very easy to be accepted.
    lower_threshold_term = g_prev;
  } else if (std::abs(t1 - t_prev) == 2 &&
             t0 > kInitialPitchPeriodThresholds[ratio - 2]) {
    // The candidate pitch period is 2 samples far from the previous one and the
    // period |t0| (from which |t1| has been derived) is greater than a
    // threshold. Make |t1| easy to be accepted.
    lower_threshold_term = 0.5f * g_prev;
  }
  // Set the threshold based on the gain of the initial estimate |t0|. Also
  // reduce the chance of false positives caused by a bias towards high
  // frequencies (originating from short-term correlations).
  float threshold = std::max(0.3f, 0.7f * g0 - lower_threshold_term);
  if (t1 < 3 * kMinPitch24kHz) {
    // High frequency.
    threshold = std::max(0.4f, 0.85f * g0 - lower_threshold_term);
  } else if (t1 < 2 * kMinPitch24kHz) {
    // Even higher frequency.
    threshold = std::max(0.5f, 0.9f * g0 - lower_threshold_term);
  }
  return threshold;
}

void ComputeSlidingFrameSquareEnergies(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
    rtc::ArrayView<float, kMaxPitch24kHz + 1> yy_values) {
  // TODO(bugs.webrtc.org/10480): Pass `yy` as argument to avoid recomputing it.
  float yy = ComputeAutoCorrelation(kMaxPitch24kHz, pitch_buf);
  yy_values[0] = yy;

  for (int i = 1; rtc::SafeLt(i, yy_values.size()); ++i) {
    RTC_DCHECK_LE(i, kMaxPitch24kHz + kFrameSize20ms24kHz);
    RTC_DCHECK_LE(i, kMaxPitch24kHz);
    // TODO(bugs.webrtc.org/10480): Switch to forward.
    const float old_coeff = pitch_buf[kBufSize24kHz - i];
    const float new_coeff = pitch_buf[kMaxPitch24kHz - i];
    yy -= old_coeff * old_coeff;
    yy += new_coeff * new_coeff;
    yy = std::max(1.f, yy);
    yy_values[i] = yy;
  }
}

CandidatePitchPeriods FindBestPitchPeriods12kHz(
    rtc::ArrayView<const float, kNumInvertedLags12kHz> auto_correlation,
    rtc::ArrayView<const float, kBufSize12kHz> pitch_buffer) {
  static_assert(kMaxPitch12kHz > kNumInvertedLags12kHz, "");
  static_assert(kMaxPitch12kHz < kBufSize12kHz, "");

  // Stores a pitch candidate period and strength information.
  struct PitchCandidate {
    // Pitch period encoded as inverted lag.
    int period_inverted_lag = 0;
    // Pitch strength encoded as a ratio.
    float strength_numerator = -1.f;
    float strength_denominator = 0.f;
    // Compare the strength of two pitch candidates.
    bool HasStrongerPitchThan(const PitchCandidate& b) const {
      // Comparing the numerator/denominator ratios without using divisions.
      return strength_numerator * b.strength_denominator >
             b.strength_numerator * strength_denominator;
    }
  };

  // TODO(bugs.webrtc.org/9076): Maybe optimize using vectorization.
  float denominator = std::inner_product(
      pitch_buffer.begin(), pitch_buffer.begin() + kFrameSize20ms12kHz + 1,
      pitch_buffer.begin(), 1.f);
  // Search best and second best pitches by looking at the scaled
  // auto-correlation.
  PitchCandidate best;
  PitchCandidate second_best;
  second_best.period_inverted_lag = 1;
  for (int inverted_lag = 0; inverted_lag < kNumInvertedLags12kHz;
       ++inverted_lag) {
    // A pitch candidate must have positive correlation.
    if (auto_correlation[inverted_lag] > 0.f) {
      PitchCandidate candidate{
          inverted_lag,
          auto_correlation[inverted_lag] * auto_correlation[inverted_lag],
          denominator};
      if (candidate.HasStrongerPitchThan(second_best)) {
        if (candidate.HasStrongerPitchThan(best)) {
          second_best = best;
          best = candidate;
        } else {
          second_best = candidate;
        }
      }
    }
    // Update |squared_energy_y| for the next inverted lag.
    const float y_old = pitch_buffer[inverted_lag];
    const float y_new = pitch_buffer[inverted_lag + kFrameSize20ms12kHz];
    denominator -= y_old * y_old;
    denominator += y_new * y_new;
    denominator = std::max(0.f, denominator);
  }
  return {best.period_inverted_lag, second_best.period_inverted_lag};
}

int RefinePitchPeriod48kHz(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer,
    rtc::ArrayView<const float, kMaxPitch24kHz + 1> y_energy,
    CandidatePitchPeriods pitch_candidates) {
  // Compute the auto-correlation terms only for neighbors of the given pitch
  // candidates (similar to what is done in ComputePitchAutoCorrelation(), but
  // for a few lag values).
  std::array<float, kNumInvertedLags24kHz> auto_correlation{};
  const Interval i1 = CreateInvertedLagInterval(pitch_candidates.best);
  const Interval i2 = CreateInvertedLagInterval(pitch_candidates.second_best);
  RTC_DCHECK_LE(i1.first, i1.last);
  RTC_DCHECK_LE(i2.first, i2.last);
  if (i1.first <= i2.first && i1.last >= i2.first) {
    // Overlapping intervals (`i1` precedes `i2`).
    RTC_DCHECK_LE(i1.last, i2.last);
    ComputeAutoCorrelation({i1.first, i2.last}, pitch_buffer, auto_correlation);
  } else if (i1.first > i2.first && i2.last >= i1.first) {
    // Overlapping intervals (`i2` precedes `i1`).
    RTC_DCHECK_LE(i2.last, i1.last);
    ComputeAutoCorrelation({i2.first, i1.last}, pitch_buffer, auto_correlation);
  } else {
    // Disjoint intervals.
    ComputeAutoCorrelation(i1, pitch_buffer, auto_correlation);
    ComputeAutoCorrelation(i2, pitch_buffer, auto_correlation);
  }
  // Find best pitch at 24 kHz.
  const int pitch_candidate_24kHz =
      FindBestPitchPeriods24kHz(auto_correlation, y_energy, pitch_buffer);
  // Pseudo-interpolation.
  return PitchPseudoInterpolationInvLagAutoCorr(pitch_candidate_24kHz,
                                                auto_correlation);
}

PitchInfo CheckLowerPitchPeriodsAndComputePitchGain(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer,
    rtc::ArrayView<const float, kMaxPitch24kHz + 1> y_energy,
    int initial_pitch_period_48kHz,
    PitchInfo prev_pitch_48kHz) {
  RTC_DCHECK_LE(kMinPitch48kHz, initial_pitch_period_48kHz);
  RTC_DCHECK_LE(initial_pitch_period_48kHz, kMaxPitch48kHz);
  // Stores information for a refined pitch candidate.
  struct RefinedPitchCandidate {
    RefinedPitchCandidate() {}
    RefinedPitchCandidate(int period_24kHz, float gain, float xy, float yy)
        : period_24kHz(period_24kHz), gain(gain), xy(xy), yy(yy) {}
    int period_24kHz;
    // Pitch strength information.
    float gain;
    // Additional pitch strength information used for the final estimation of
    // pitch gain.
    float xy;  // Cross-correlation.
    float yy;  // Auto-correlation.
  };

  // Initialize.
  const float xx = y_energy[0];
  // Helper lambdas.
  const auto pitch_gain = [](float xy, float yy, float xx) {
    RTC_DCHECK_LE(0.f, xx * yy);
    return xy / std::sqrt(1.f + xx * yy);
  };
  // Initial pitch candidate gain.
  RefinedPitchCandidate best_pitch;
  best_pitch.period_24kHz =
      std::min(initial_pitch_period_48kHz / 2, kMaxPitch24kHz - 1);
  best_pitch.xy = ComputeAutoCorrelation(
      GetInvertedLag(best_pitch.period_24kHz), pitch_buffer);
  best_pitch.yy = y_energy[best_pitch.period_24kHz];
  best_pitch.gain = pitch_gain(best_pitch.xy, best_pitch.yy, xx);

  // Store the initial pitch period information.
  const int initial_pitch_period = best_pitch.period_24kHz;
  const float initial_pitch_gain = best_pitch.gain;

  // Given the initial pitch estimation, check lower periods (i.e., harmonics).
  const auto alternative_period = [](int period, int k, int n) -> int {
    RTC_DCHECK_GT(k, 0);
    return (2 * n * period + k) / (2 * k);  // Same as round(n*period/k).
  };
  // |max_k| such that alternative_period(initial_pitch_period, max_k, 1) equals
  // kMinPitch24kHz.
  const int max_k = (2 * initial_pitch_period) / (2 * kMinPitch24kHz - 1);
  for (int k = 2; k <= max_k; ++k) {
    int candidate_pitch_period = alternative_period(initial_pitch_period, k, 1);
    RTC_DCHECK_GE(candidate_pitch_period, kMinPitch24kHz);
    // When looking at |candidate_pitch_period|, we also look at one of its
    // sub-harmonics. |kSubHarmonicMultipliers| is used to know where to look.
    // |k| == 2 is a special case since |candidate_pitch_secondary_period| might
    // be greater than the maximum pitch period.
    int candidate_pitch_secondary_period = alternative_period(
        initial_pitch_period, k, kSubHarmonicMultipliers[k - 2]);
    RTC_DCHECK_GT(candidate_pitch_secondary_period, 0);
    if (k == 2 && candidate_pitch_secondary_period > kMaxPitch24kHz) {
      candidate_pitch_secondary_period = initial_pitch_period;
    }
    RTC_DCHECK_NE(candidate_pitch_period, candidate_pitch_secondary_period)
        << "The lower pitch period and the additional sub-harmonic must not "
           "coincide.";
    // Compute an auto-correlation score for the primary pitch candidate
    // |candidate_pitch_period| by also looking at its possible sub-harmonic
    // |candidate_pitch_secondary_period|.
    float xy_primary_period = ComputeAutoCorrelation(
        GetInvertedLag(candidate_pitch_period), pitch_buffer);
    float xy_secondary_period = ComputeAutoCorrelation(
        GetInvertedLag(candidate_pitch_secondary_period), pitch_buffer);
    float xy = 0.5f * (xy_primary_period + xy_secondary_period);
    float yy = 0.5f * (y_energy[candidate_pitch_period] +
                       y_energy[candidate_pitch_secondary_period]);
    float candidate_pitch_gain = pitch_gain(xy, yy, xx);

    // Maybe update best period.
    float threshold = ComputePitchGainThreshold(
        candidate_pitch_period, k, initial_pitch_period, initial_pitch_gain,
        prev_pitch_48kHz.period / 2, prev_pitch_48kHz.gain);
    if (candidate_pitch_gain > threshold) {
      best_pitch = {candidate_pitch_period, candidate_pitch_gain, xy, yy};
    }
  }

  // Final pitch gain and period.
  best_pitch.xy = std::max(0.f, best_pitch.xy);
  RTC_DCHECK_LE(0.f, best_pitch.yy);
  float final_pitch_gain = (best_pitch.yy <= best_pitch.xy)
                               ? 1.f
                               : best_pitch.xy / (best_pitch.yy + 1.f);
  final_pitch_gain = std::min(best_pitch.gain, final_pitch_gain);
  int final_pitch_period_48kHz =
      std::max(kMinPitch48kHz, PitchPseudoInterpolationLagPitchBuf(
                                   best_pitch.period_24kHz, pitch_buffer));

  return {final_pitch_period_48kHz, final_pitch_gain};
}

}  // namespace rnn_vad
}  // namespace webrtc
