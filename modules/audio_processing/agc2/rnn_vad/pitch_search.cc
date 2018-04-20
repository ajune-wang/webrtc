/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/pitch_search.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// Converts a lag to an inverted lag (only for 24kHz).
size_t GetInvertedLag(size_t lag) {
  RTC_DCHECK_LE(lag, kPitchMaxPeriod24kHz);
  return kPitchMaxPeriod24kHz - lag;
}

float ComputeAutoCorrelationCoeff(rtc::ArrayView<const float> pitch_buf,
                                  size_t inv_lag,
                                  size_t max_pitch_period) {
  RTC_DCHECK_LT(inv_lag, pitch_buf.size());
  RTC_DCHECK_LT(max_pitch_period, pitch_buf.size());
  RTC_DCHECK_LE(inv_lag, max_pitch_period);
  // TODO(bugs.webrtc.org/8948): Maybe optimize using vectorization.
  return std::inner_product(pitch_buf.begin() + max_pitch_period,
                            pitch_buf.end(), pitch_buf.begin() + inv_lag, 0.f);
}

// Refines a pitch period |lag| with pseudo-interpolation. Return the refined
// lag at 48 kHz.
size_t PitchPseudoInterpolationLag(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
    size_t lag) {
  const float a = ComputeAutoCorrelationCoeff(
      pitch_buf, GetInvertedLag(lag - 1), kPitchMaxPeriod24kHz);
  const float b = ComputeAutoCorrelationCoeff(pitch_buf, GetInvertedLag(lag),
                                              kPitchMaxPeriod24kHz);
  const float c = ComputeAutoCorrelationCoeff(
      pitch_buf, GetInvertedLag(lag + 1), kPitchMaxPeriod24kHz);

  int offset = 0;
  if ((c - a) > 0.7f * (b - a)) {
    offset = 1;  // |c| is the largest auto-correlation coefficient.
  } else if ((a - c) > 0.7f * (b - c)) {
    offset = -1;  // |a| is the largest auto-correlation coefficient.
  }
  return 2 * lag + offset;
}

// Refines a pitch period |inv_lag| encoded as inverted lag with
// pseudo-interpolation. The output sample rate is as twice as that of
// |inv_lag|.
size_t PitchPseudoInterpolationInvLag(rtc::ArrayView<const float> auto_corr,
                                      size_t inv_lag) {
  // Cannot apply pseudo-interpolation at the boundaries.
  if (inv_lag == 0 || inv_lag >= auto_corr.size() - 1)
    return 2 * inv_lag;

  const float a = auto_corr[inv_lag - 1];
  const float b = auto_corr[inv_lag];
  const float c = auto_corr[inv_lag + 1];

  int offset = 0;
  if ((c - a) > 0.7f * (b - a)) {
    offset = 1;  // |c| is the largest auto-correlation coefficient.
  } else if ((a - c) > 0.7f * (b - c)) {
    offset = -1;  // |a| is the largest auto-correlation coefficient.
  }
  // TODO(bugs.webrtc.org/8948): When retraining, check if below
  // |offset| should be summed since this function returns an inverted lag.
  return 2 * inv_lag - offset;
}

// Integer multipliers used in CheckLowerPitchPeriodsAndComputePitchGain() when
// looking for sub-harmonics.
// The values have been found as follows. Given the initial pitch period T, we
// look at shorter periods (or its harmonics) by considering T/k with k in
// {2, ..., 15}. When for example k = 4, we should also expect a peak at T*3/4.
// When k = 8 instead we don't want to look at T*2/8, since we have already
// checked T/4 before. Instead, we look at T*3/8. The values are hand-tuned in
// order to look at peaks that we would not expect for a different pitch.
constexpr std::array<size_t, 14> kSubHarmonicMultipliers = {
    3, 2, 3, 2, 5, 2, 3, 2, 3, 2, 5, 2, 3, 2};

// Initial pitch period candidate thresholds for ComputePitchGainThreshold() for
// a sample rate of 24 kHz. Computed as [5*k*k for k in range(16)].
constexpr std::array<size_t, 14> kInitialPitchPeriodThresholds = {
    20, 45, 80, 125, 180, 245, 320, 405, 500, 605, 720, 845, 980, 1125};

}  // namespace

namespace impl {

void Decimate2x(rtc::ArrayView<const float, kBufSize24kHz> src,
                rtc::ArrayView<float, kBufSize12kHz> dst) {
  static_assert(2 * dst.size() == src.size(), "");
  for (size_t i = 0; i < dst.size(); ++i)
    dst[i] = src[2 * i];
}

float ComputePitchGainThreshold(size_t candidate_pitch_period,
                                size_t pitch_period_ratio,
                                size_t initial_pitch_period,
                                float initial_pitch_gain,
                                size_t prev_pitch_period,
                                size_t prev_pitch_gain) {
  // Map arguments to more compact aliases.
  size_t& t1 = candidate_pitch_period;
  size_t& k = pitch_period_ratio;
  size_t& t0 = initial_pitch_period;
  float& g0 = initial_pitch_gain;
  size_t& t_prev = prev_pitch_period;
  size_t& g_prev = prev_pitch_gain;

  // Validate input.
  RTC_DCHECK_GE(k, 2);

  // Compute a term that lowers the threshold when |t1| is close to the last
  // estimated period |t_prev| - i.e., pitch tracking.
  float lower_threshold_term = 0;
  if (abs(static_cast<int>(t1) - static_cast<int>(t_prev)) <= 1) {
    // The candidate pitch period is within 1 sample from the previous one.
    // Make the candidate at |t1| very easy to be accepted.
    lower_threshold_term = g_prev;
  } else if (abs(static_cast<int>(t1) - static_cast<int>(t_prev)) == 2 &&
             t0 > kInitialPitchPeriodThresholds[k - 2]) {
    // The candidate pitch period is 2 samples far from the previous one and the
    // period |t0| (from which |t1| has been derived) is greater than a
    // threshold. Make |t1| easy to be accepted.
    lower_threshold_term = 0.5f * g_prev;
  }
  // Set the threshold based on the gain of the initial estimate |t0|. Also
  // reduce the chance of false positives caused by a bias towards high
  // frequencies (originating from short-term correlations).
  float threshold = std::max(0.3f, 0.7f * g0 - lower_threshold_term);
  if (t1 < 3 * kPitchMinPeriod24kHz) {  // High frequency.
    threshold = std::max(0.4f, 0.85f * g0 - lower_threshold_term);
  } else if (t1 < 2 * kPitchMinPeriod24kHz) {  // Even higher frequency.
    threshold = std::max(0.5f, 0.9f * g0 - lower_threshold_term);
  }
  return threshold;
}

void ComputeSlidingFrameSquareEnergies(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
    rtc::ArrayView<float, kPitchMaxPeriod24kHz + 1> yy_values) {
  float yy = ComputeAutoCorrelationCoeff(pitch_buf, kPitchMaxPeriod24kHz,
                                         kPitchMaxPeriod24kHz);
  yy_values[0] = yy;
  for (size_t i = 1; i < yy_values.size(); ++i) {
    RTC_DCHECK_LE(i, kPitchMaxPeriod24kHz + kFrameSize20ms24kHz);
    RTC_DCHECK_LE(i, kPitchMaxPeriod24kHz);
    const float old_coeff =
        pitch_buf[kPitchMaxPeriod24kHz + kFrameSize20ms24kHz - i];
    const float new_coeff = pitch_buf[kPitchMaxPeriod24kHz - i];
    yy -= old_coeff * old_coeff;
    yy += new_coeff * new_coeff;
    yy_values[i] = std::max(0.f, yy);
  }
}

void ComputePitchAutoCorrelation(
    rtc::ArrayView<const float, kBufSize12kHz> pitch_buf,
    size_t max_pitch_period,
    rtc::ArrayView<float, impl::kNumInvertedLags12kHz> auto_corr) {
  RTC_DCHECK_GT(max_pitch_period, auto_corr.size());
  RTC_DCHECK_LT(max_pitch_period, pitch_buf.size());
  // Compute auto-correlation coefficients.
  for (size_t inv_lag = 0; inv_lag < auto_corr.size(); ++inv_lag) {
    auto_corr[inv_lag] =
        ComputeAutoCorrelationCoeff(pitch_buf, inv_lag, max_pitch_period);
  }
}

std::array<size_t, 2> FindBestPitchPeriods(
    rtc::ArrayView<const float> auto_corr,
    rtc::ArrayView<const float> pitch_buf,
    size_t max_pitch_period) {
  // Stores a pitch candidate period and strength information.
  struct PitchCandidate {
    // Pitch period encoded as inverted lag.
    size_t period_inverted_lag = 0;
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

  RTC_DCHECK_GT(max_pitch_period, auto_corr.size());
  RTC_DCHECK_LT(max_pitch_period, pitch_buf.size());
  const size_t frame_size = pitch_buf.size() - max_pitch_period;
  // TODO(bugs.webrtc.org/8948): Maybe optimize using vectorization.
  float yy =
      std::inner_product(pitch_buf.begin(), pitch_buf.begin() + frame_size + 1,
                         pitch_buf.begin(), 1.f);
  // Search best and second best pitches by looking at the scaled
  // auto-correlation.
  PitchCandidate candidate;
  PitchCandidate best;
  PitchCandidate second_best;
  second_best.period_inverted_lag = 1;
  for (size_t inv_lag = 0; inv_lag < auto_corr.size(); ++inv_lag) {
    // A pitch candidate must have positive correlation.
    if (auto_corr[inv_lag] > 0) {
      candidate.period_inverted_lag = inv_lag;
      candidate.strength_numerator = auto_corr[inv_lag] * auto_corr[inv_lag];
      candidate.strength_denominator = yy;
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
    const float old_coeff = pitch_buf[inv_lag];
    const float new_coeff = pitch_buf[inv_lag + frame_size];
    yy -= old_coeff * old_coeff;
    yy += new_coeff * new_coeff;
    yy = std::max(0.f, yy);
  }
  return {best.period_inverted_lag, second_best.period_inverted_lag};
}

size_t RefinePitchPeriod48kHz(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
    rtc::ArrayView<const size_t, 2> inv_lags) {
  // Compute the auto-correlation terms only for neighbors of the given pitch
  // candidates (similar to what is done in ComputePitchAutoCorrelation(), but
  // for a few lag values).
  std::array<float, impl::kNumInvertedLags24kHz> auto_corr;
  auto_corr.fill(0.f);  // Zeros become ignored lags in FindBestPitchPeriods().
  auto is_neighbor = [](int i, int j) { return abs(i - j) <= 2; };
  for (size_t inv_lag = 0; inv_lag < auto_corr.size(); ++inv_lag) {
    if (is_neighbor(inv_lag, inv_lags[0]) || is_neighbor(inv_lag, inv_lags[1]))
      auto_corr[inv_lag] =
          ComputeAutoCorrelationCoeff(pitch_buf, inv_lag, kPitchMaxPeriod24kHz);
  }
  // Find best pitch at 24 kHz.
  const auto pitch_candidates_inv_lags = FindBestPitchPeriods(
      {auto_corr.data(), auto_corr.size()},
      {pitch_buf.data(), pitch_buf.size()}, kPitchMaxPeriod24kHz);
  const auto inv_lag = pitch_candidates_inv_lags[0];  // Refine the best.
  // Pseudo-interpolation.
  return PitchPseudoInterpolationInvLag({auto_corr.data(), auto_corr.size()},
                                        inv_lag);
}

PitchInfo CheckLowerPitchPeriodsAndComputePitchGain(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
    size_t initial_pitch_period_48kHz,
    PitchInfo prev_pitch_48kHz) {
  RTC_DCHECK_LE(kPitchMinPeriod48kHz, initial_pitch_period_48kHz);
  RTC_DCHECK_LE(initial_pitch_period_48kHz, kPitchMaxPeriod48kHz);
  // Stores information for a refined pitch candidate.
  struct RefinedPitchCandidate {
    RefinedPitchCandidate() {}
    RefinedPitchCandidate(size_t new_period,
                          float new_gain,
                          float new_xy,
                          float new_yy)
        : period(new_period), gain(new_gain), xy(new_xy), yy(new_yy) {}
    size_t period;
    // Pitch strength information.
    float gain;
    // Additional pitch strength information used for the final estimation of
    // pitch gain.
    float xy;  // Cross-correlation.
    float yy;  // Auto-correlation.
  };

  // Initialize.
  std::array<float, kPitchMaxPeriod24kHz + 1> yy_values;
  ComputeSlidingFrameSquareEnergies(pitch_buf,
                                    {yy_values.data(), yy_values.size()});
  const float xx = yy_values[0];
  // Helper lambdas.
  auto pitch_gain = [](float xy, float yy, float xx) {
    RTC_DCHECK_NE(-1.f, xx * yy);
    return xy / std::sqrt(1.f + xx * yy);
  };
  // Initial pitch candidate gain.
  RefinedPitchCandidate best_pitch;
  best_pitch.period =
      std::min(initial_pitch_period_48kHz / 2, kPitchMaxPeriod24kHz - 1);
  best_pitch.xy = ComputeAutoCorrelationCoeff(
      pitch_buf, GetInvertedLag(best_pitch.period), kPitchMaxPeriod24kHz);
  best_pitch.yy = yy_values[best_pitch.period];
  best_pitch.gain = pitch_gain(best_pitch.xy, best_pitch.yy, xx);

  // Store the initial pitch period information.
  const size_t initial_pitch_period = best_pitch.period;
  const float initial_pitch_gain = best_pitch.gain;

  // Given the initial pitch estimation, check lower periods (i.e., harmonics).
  auto alternative_period = [](size_t t, size_t k, size_t n) {
    return (2 * n * t + k) / (2 * k);  // Same as round(n*t/k).
  };
  for (size_t k = 2; k < kSubHarmonicMultipliers.size() + 2; ++k) {
    const size_t candidate_pitch_period =
        alternative_period(initial_pitch_period, k, 1);
    if (candidate_pitch_period < kPitchMinPeriod24kHz)
      break;
    // When looking at |candidate_pitch_period|, we also look at one of its
    // sub-harmonics. |kSubHarmonicMultipliers| is used to know where to look.
    // |k| == 2 is a special case since |candidate_pitch_secondary_period| might
    // be greater than the maximum pitch period.
    size_t candidate_pitch_secondary_period = alternative_period(
        initial_pitch_period, k, kSubHarmonicMultipliers[k - 2]);
    if (k == 2 && candidate_pitch_secondary_period > kPitchMaxPeriod24kHz)
      candidate_pitch_secondary_period = initial_pitch_period;
    RTC_DCHECK_NE(candidate_pitch_period, candidate_pitch_secondary_period)
        << "The lower pitch period and the additional sub-harmonic must not "
        << "coincide.";
    // Compute an auto-correlation score for the primary pitch candidate
    // |candidate_pitch_period| by also looking at its possible sub-harmonic
    // |candidate_pitch_secondary_period|.
    const float xy_primary_period = ComputeAutoCorrelationCoeff(
        pitch_buf, GetInvertedLag(candidate_pitch_period),
        kPitchMaxPeriod24kHz);
    const float xy_secondary_period = ComputeAutoCorrelationCoeff(
        pitch_buf, GetInvertedLag(candidate_pitch_secondary_period),
        kPitchMaxPeriod24kHz);
    const float xy = 0.5f * (xy_primary_period + xy_secondary_period);
    const float yy = 0.5f * (yy_values[candidate_pitch_period] +
                             yy_values[candidate_pitch_secondary_period]);
    const float candidate_pitch_gain = pitch_gain(xy, yy, xx);

    // Maybe update best period.
    const float threshold = ComputePitchGainThreshold(
        candidate_pitch_period, k, initial_pitch_period, initial_pitch_gain,
        prev_pitch_48kHz.period / 2, prev_pitch_48kHz.gain);
    if (candidate_pitch_gain > threshold) {
      best_pitch = {candidate_pitch_period, candidate_pitch_gain, xy, yy};
    }
  }

  // Final pitch gain and period.
  best_pitch.xy = std::max(0.f, best_pitch.xy);
  float final_pitch_gain =
      std::min(best_pitch.gain, (best_pitch.yy <= best_pitch.xy)
                                    ? 1.f
                                    : best_pitch.xy / (best_pitch.yy + 1.f));
  size_t final_pitch_period_48kHz =
      std::max(kPitchMinPeriod48kHz,
               PitchPseudoInterpolationLag(pitch_buf, best_pitch.period));

  return {final_pitch_period_48kHz, final_pitch_gain};
}

}  // namespace impl

PitchInfo PitchSearch(rtc::ArrayView<const float, kBufSize24kHz> pitch_buf,
                      PitchInfo prev_pitch_48kHz) {
  // Perform the initial pitch search at 12 kHz.
  std::array<float, kBufSize12kHz> pitch_buf_decimated;
  impl::Decimate2x(pitch_buf,
                   {pitch_buf_decimated.data(), pitch_buf_decimated.size()});
  // Compute auto-correlation terms.
  std::array<float, impl::kNumInvertedLags12kHz> auto_corr;
  impl::ComputePitchAutoCorrelation(
      {pitch_buf_decimated.data(), pitch_buf_decimated.size()},
      kPitchMaxPeriod12kHz, {auto_corr.data(), auto_corr.size()});
  // Search pitch at 12 kHz.
  std::array<size_t, 2> pitch_candidates_inv_lags = impl::FindBestPitchPeriods(
      {auto_corr.data(), auto_corr.size()},
      {pitch_buf_decimated.data(), pitch_buf_decimated.size()},
      kPitchMaxPeriod12kHz);

  // Refine the pitch period estimation.
  // The refinement is done using the pitch buffer that contains 24 kHz samples.
  // Therefore, adapt the inverted lags in |pitch_candidates_inv_lags| from 12
  // to 24 kHz.
  for (size_t i = 0; i < pitch_candidates_inv_lags.size(); ++i)
    pitch_candidates_inv_lags[i] *= 2;
  const size_t pitch_inv_lag_48kHz = impl::RefinePitchPeriod48kHz(
      pitch_buf,
      {pitch_candidates_inv_lags.data(), pitch_candidates_inv_lags.size()});
  // Look for stronger harmonics to find the final pitch period and its gain.
  RTC_DCHECK_LT(pitch_inv_lag_48kHz, kPitchMaxPeriod48kHz);
  return impl::CheckLowerPitchPeriodsAndComputePitchGain(
      pitch_buf, kPitchMaxPeriod48kHz - pitch_inv_lag_48kHz, prev_pitch_48kHz);
}

}  // namespace rnn_vad
}  // namespace webrtc
