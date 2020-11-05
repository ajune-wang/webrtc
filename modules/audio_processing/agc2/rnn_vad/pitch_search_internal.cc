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
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_compare.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace rnn_vad {
namespace {

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

// Given an auto-correlation coefficient `curr_auto_correlation` and its
// neighboring values `prev_auto_correlation` and `next_auto_correlation`
// computes a pseudo-interpolation offset to be applied to the pitch period
// associated to `curr`. The output is a lag in {-1, 0, +1}.
// TODO(bugs.webrtc.org/9076): Consider removing this method.
// `GetPitchPseudoInterpolationOffset()` it is relevant only if the spectral
// analysis works at a sample rate that is twice as that of the pitch buffer;
// In particular, it is not relevant for the estimated pitch period feature fed
// into the RNN.
int GetPitchPseudoInterpolationOffset(float prev_auto_correlation,
                                      float curr_auto_correlation,
                                      float next_auto_correlation) {
  if ((next_auto_correlation - prev_auto_correlation) >
      0.7f * (curr_auto_correlation - prev_auto_correlation)) {
    return 1;  // |next_auto_correlation| is the largest auto-correlation
               // coefficient.
  } else if ((prev_auto_correlation - next_auto_correlation) >
             0.7f * (curr_auto_correlation - next_auto_correlation)) {
    return -1;  // |prev_auto_correlation| is the largest auto-correlation
                // coefficient.
  }
  return 0;
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
        ComputeAutoCorrelation(kMaxPitch24kHz - (lag - 1), pitch_buffer),
        ComputeAutoCorrelation(kMaxPitch24kHz - lag, pitch_buffer),
        ComputeAutoCorrelation(kMaxPitch24kHz - (lag + 1), pitch_buffer));
  }
  return 2 * lag + offset;
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

struct Range {
  int min;
  int max;
};

// Number of analyzed pitches to the left(right) of a pitch candidate.
constexpr int kPitchNeighborhoodRadius = 2;

// Creates a pitch period interval centered in `inverted_lag` with hard-coded
// radius. Clipping is applied so that the interval is always valid for a 24 kHz
// pitch buffer.
Range CreateInvertedLagRange(int inverted_lag) {
  return {std::max(inverted_lag - kPitchNeighborhoodRadius, 0),
          std::min(inverted_lag + kPitchNeighborhoodRadius,
                   kNumInvertedLags24kHz - 1)};
}

constexpr int kNumPitchCandidates = 2;  // Best and second best.
// Maximum number of analyzed pitch periods.
constexpr int kMaxPitchPeriods24kHz =
    kNumPitchCandidates * (2 * kPitchNeighborhoodRadius + 1);

// Collection of inverted lags.
class InvertedLagsIndex {
 public:
  InvertedLagsIndex() : num_entries_(0) {}
  // Adds an inverted lag to the index. Cannot add more than
  // `kMaxPitchPeriods24kHz` values.
  void Append(int inverted_lag) {
    RTC_DCHECK_LT(num_entries_, inverted_lags_.size());
    if (num_entries_ == kMaxPitchPeriods24kHz) {
      RTC_LOG(LS_ERROR) << "Full sparse auto correlation collection.";
      return;
    }
    inverted_lags_[num_entries_++] = inverted_lag;
  }
  int size() const { return num_entries_; }
  rtc::ArrayView<const int> View() const {
    return {inverted_lags_.data(), static_cast<size_t>(num_entries_)};
  }

 private:
  std::array<int, kMaxPitchPeriods24kHz> inverted_lags_;
  int num_entries_;
};

// Computes the auto correlation coefficients for the inverted lags in the
// closed interval `inverted_lags`. Updates `inverted_lags_index` by appending
// the inverted lags for the computed auto correlation values.
void ComputeAutoCorrelation(
    Range inverted_lags,
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer,
    rtc::ArrayView<float, kNumInvertedLags24kHz> auto_correlation,
    InvertedLagsIndex& inverted_lags_index) {
  // Trick to avoid zero initialization of `auto_correlation`.
  // Needed by the pseudo-interpolation.
  if (inverted_lags.min > 0) {
    auto_correlation[inverted_lags.min - 1] = 0.f;
  }
  if (inverted_lags.max < kNumInvertedLags24kHz - 1) {
    auto_correlation[inverted_lags.max + 1] = 0.f;
  }
  for (int inverted_lag = inverted_lags.min; inverted_lag <= inverted_lags.max;
       ++inverted_lag) {
    auto_correlation[inverted_lag] =
        ComputeAutoCorrelation(inverted_lag, pitch_buffer);
    inverted_lags_index.Append(inverted_lag);
  }
}

// Searches the best pitch period at 24 kHz and returns its inverted lag at 48
// kHz.
int FindBestPitchPeriod48kHz(
    rtc::ArrayView<const float, kNumInvertedLags24kHz> auto_correlation,
    rtc::ArrayView<const int> inverted_lags,
    rtc::ArrayView<const float, kMaxPitch24kHz + 1> y_energy,
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer) {
  static_assert(kMaxPitch24kHz > kNumInvertedLags24kHz, "");
  static_assert(kMaxPitch24kHz < kBufSize24kHz, "");
  int best_inverted_lag = 0;     // Pitch period.
  float best_numerator = -1.f;   // Pitch strength numerator.
  float best_denominator = 0.f;  // Pitch strength denominator.
  for (int inverted_lag : inverted_lags) {
    if (auto_correlation[inverted_lag] > 0.f) {
      // Auto-correlation energy normalized by frame energy.
      const float numerator =
          auto_correlation[inverted_lag] * auto_correlation[inverted_lag];
      const float denominator = y_energy[inverted_lag];
      // Compare numerator/denominator ratios without using divisions.
      if (numerator * best_denominator > best_numerator * denominator) {
        best_inverted_lag = inverted_lag;
        best_numerator = numerator;
        best_denominator = denominator;
      }
    }
  }
  // Pseudo-interpolation to transform `best_inverted_lag` (24 kHz pitch) to a
  // 48 kHz pitch period.
  if (best_inverted_lag == 0 ||
      best_inverted_lag >= kNumInvertedLags24kHz - 1) {
    // Cannot apply pseudo-interpolation at the boundaries.
    return best_inverted_lag * 2;
  }
  int offset = GetPitchPseudoInterpolationOffset(
      auto_correlation[best_inverted_lag + 1],
      auto_correlation[best_inverted_lag],
      auto_correlation[best_inverted_lag - 1]);
  // TODO(bugs.webrtc.org/9076): When retraining, check if |offset| below should
  // be subtracted since |inverted_lag| is an inverted lag but offset is a lag.
  return 2 * best_inverted_lag + offset;
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

// Returns an alternative pitch period for `pitch_period` which is computed as
// `round(n * pitch_period / k)`.
constexpr int GetAlternativePitchPeriod(int pitch_period, int n, int k) {
  RTC_DCHECK_GT(k, 0);
  return (2 * n * pitch_period + k) / (2 * k);
}

// Returns true if the alternative pitch period is stronger than the initial one
// given the last estimated pitch and the factor `k` used to compute the
// alternative pitch period via `GetAlternativePitchPeriod()`.
bool IsAlternativePitchStrongerThanInitial(PitchInfo last,
                                           PitchInfo initial,
                                           PitchInfo alternative,
                                           int k) {
  RTC_DCHECK_GE(last.period, 0);
  RTC_DCHECK_GE(initial.period, 0);
  RTC_DCHECK_GE(alternative.period, 0);
  RTC_DCHECK_GE(k, 2);
  // Compute a term that lowers the threshold when |alternative.period| is close
  // to the last estimated period |last.period| - i.e., pitch tracking.
  float lower_threshold_term = 0.f;
  if (std::abs(alternative.period - last.period) <= 1) {
    // The candidate pitch period is within 1 sample from the last one.
    // Make the candidate at |alternative.period| very easy to be accepted.
    lower_threshold_term = last.gain;
  } else if (std::abs(alternative.period - last.period) == 2 &&
             initial.period > kInitialPitchPeriodThresholds[k - 2]) {
    // The candidate pitch period is 2 samples far from the last one and the
    // period |initial.period| (from which |alternative.period| has been
    // derived) is greater than a threshold. Make |alternative.period| easy to
    // be accepted.
    lower_threshold_term = 0.5f * last.gain;
  }
  // Set the threshold based on the gain of the initial estimate
  // |initial.period|. Also reduce the chance of false positives caused by a
  // bias towards high frequencies (originating from short-term correlations).
  float threshold = std::max(0.3f, 0.7f * initial.gain - lower_threshold_term);
  if (alternative.period < 3 * kMinPitch24kHz) {
    // High frequency.
    threshold = std::max(0.4f, 0.85f * initial.gain - lower_threshold_term);
  } else if (alternative.period < 2 * kMinPitch24kHz) {
    // Even higher frequency.
    threshold = std::max(0.5f, 0.9f * initial.gain - lower_threshold_term);
  }
  return alternative.gain > threshold;
}

void ComputeSlidingFrameSquareEnergies(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer,
    rtc::ArrayView<float, kMaxPitch24kHz + 1> yy_values) {
  float yy = std::inner_product(pitch_buffer.begin(),
                                pitch_buffer.begin() + kFrameSize20ms24kHz,
                                pitch_buffer.begin(), 0.f);
  yy_values[0] = yy;
  for (int i = 0; i < kMaxPitch24kHz; ++i) {
    yy -= pitch_buffer[i] * pitch_buffer[i];
    yy += pitch_buffer[i + kFrameSize20ms24kHz] *
          pitch_buffer[i + kFrameSize20ms24kHz];
    yy = std::max(1.f, yy);
    yy_values[i + 1] = yy;
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
  // Compute the auto-correlation terms only for neighbors of the two pitch
  // candidates (best and second best).
  std::array<float, kNumInvertedLags24kHz> auto_correlation;
  InvertedLagsIndex inverted_lags_index;
  const Range i1 = CreateInvertedLagRange(pitch_candidates.best);
  const Range i2 = CreateInvertedLagRange(pitch_candidates.second_best);
  RTC_DCHECK_LE(i1.min, i1.max);
  RTC_DCHECK_LE(i2.min, i2.max);
  if (i1.min <= i2.min && i1.max + 1 >= i2.min) {
    // Overlapping or adjacent intervals (`i1` precedes `i2`).
    RTC_DCHECK_LE(i1.max, i2.max);
    ComputeAutoCorrelation({i1.min, i2.max}, pitch_buffer, auto_correlation,
                           inverted_lags_index);
  } else if (i1.min > i2.min && i2.max + 1 >= i1.min) {
    // Overlapping or adjacent intervals (`i2` precedes `i1`).
    RTC_DCHECK_LE(i2.max, i1.max);
    ComputeAutoCorrelation({i2.min, i1.max}, pitch_buffer, auto_correlation,
                           inverted_lags_index);
  } else {
    // Disjoint intervals.
    ComputeAutoCorrelation(i1, pitch_buffer, auto_correlation,
                           inverted_lags_index);
    ComputeAutoCorrelation(i2, pitch_buffer, auto_correlation,
                           inverted_lags_index);
  }
  return FindBestPitchPeriod48kHz(auto_correlation, inverted_lags_index.View(),
                                  y_energy, pitch_buffer);
}

PitchInfo CheckLowerPitchPeriodsAndComputePitchGain(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer,
    rtc::ArrayView<const float, kMaxPitch24kHz + 1> y_energy,
    int initial_pitch_period_48kHz,
    PitchInfo last_pitch_48kHz) {
  RTC_DCHECK_LE(kMinPitch48kHz, initial_pitch_period_48kHz);
  RTC_DCHECK_LE(initial_pitch_period_48kHz, kMaxPitch48kHz);

  // Stores information for a refined pitch candidate.
  struct RefinedPitchCandidate {
    int period;  // Pitch period.
    float gain;  // Pitch strength information.
    // Additional pitch strength information used for the final estimation of
    // pitch gain.
    float xy;  // Cross-correlation.
    float yy;  // Auto-correlation.
  };

  // Initialize.
  const float xx = y_energy[kMaxPitch24kHz];
  // Helpers.
  const auto pitch_gain = [](float xy, float yy, float xx) {
    RTC_DCHECK_LE(0.f, xx * yy);
    return xy / std::sqrt(1.f + xx * yy);
  };
  // Initial pitch candidate gain.
  RefinedPitchCandidate best_pitch;
  best_pitch.period =
      std::min(initial_pitch_period_48kHz / 2, kMaxPitch24kHz - 1);
  best_pitch.xy =
      ComputeAutoCorrelation(kMaxPitch24kHz - best_pitch.period, pitch_buffer);
  best_pitch.yy = y_energy[kMaxPitch24kHz - best_pitch.period];
  best_pitch.gain = pitch_gain(best_pitch.xy, best_pitch.yy, xx);

  // 24 kHz version of the last estimated pitch and copy of the initial
  // estimation.
  const PitchInfo last_pitch{last_pitch_48kHz.period / 2,
                             last_pitch_48kHz.gain};
  const PitchInfo initial_pitch{best_pitch.period, best_pitch.gain};

  // |max_k| such that alternative_period(initial_pitch_period, max_k, 1) equals
  // kMinPitch24kHz.
  const int max_k = (2 * initial_pitch.period) / (2 * kMinPitch24kHz - 1);
  for (int k = 2; k <= max_k; ++k) {
    PitchInfo alternative_pitch;
    alternative_pitch.period =
        GetAlternativePitchPeriod(initial_pitch.period, 1, k);
    RTC_DCHECK_GE(alternative_pitch.period, kMinPitch24kHz);
    // When looking at |alternative_pitch.period|, we also look at one of its
    // sub-harmonics. |kSubHarmonicMultipliers| is used to know where to look.
    // |k| == 2 is a special case since |dual_alternative_period| might be
    // greater than the maximum pitch period.
    int dual_alternative_period = GetAlternativePitchPeriod(
        initial_pitch.period, kSubHarmonicMultipliers[k - 2], k);
    RTC_DCHECK_GT(dual_alternative_period, 0);
    if (k == 2 && dual_alternative_period > kMaxPitch24kHz) {
      dual_alternative_period = initial_pitch.period;
    }
    RTC_DCHECK_NE(alternative_pitch.period, dual_alternative_period)
        << "The lower pitch period and the additional sub-harmonic must not "
           "coincide.";
    // Compute an auto-correlation score for the primary pitch candidate
    // |alternative_pitch.period| by also looking at its possible sub-harmonic
    // |dual_alternative_period|.
    float xy_primary_period = ComputeAutoCorrelation(
        kMaxPitch24kHz - alternative_pitch.period, pitch_buffer);
    float xy_secondary_period = ComputeAutoCorrelation(
        kMaxPitch24kHz - dual_alternative_period, pitch_buffer);
    float xy = 0.5f * (xy_primary_period + xy_secondary_period);
    float yy = 0.5f * (y_energy[kMaxPitch24kHz - alternative_pitch.period] +
                       y_energy[kMaxPitch24kHz - dual_alternative_period]);
    alternative_pitch.gain = pitch_gain(xy, yy, xx);

    // Maybe update best period.
    if (IsAlternativePitchStrongerThanInitial(last_pitch, initial_pitch,
                                              alternative_pitch, k)) {
      best_pitch = {alternative_pitch.period, alternative_pitch.gain, xy, yy};
    }
  }

  // Final pitch gain and period.
  best_pitch.xy = std::max(0.f, best_pitch.xy);
  RTC_DCHECK_LE(0.f, best_pitch.yy);
  float final_pitch_gain = (best_pitch.yy <= best_pitch.xy)
                               ? 1.f
                               : best_pitch.xy / (best_pitch.yy + 1.f);
  final_pitch_gain = std::min(best_pitch.gain, final_pitch_gain);
  int final_pitch_period_48kHz = std::max(
      kMinPitch48kHz,
      PitchPseudoInterpolationLagPitchBuf(best_pitch.period, pitch_buffer));

  return {final_pitch_period_48kHz, final_pitch_gain};
}

}  // namespace rnn_vad
}  // namespace webrtc
