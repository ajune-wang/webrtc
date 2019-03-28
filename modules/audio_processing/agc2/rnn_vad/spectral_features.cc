/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/spectral_features.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr float kSilenceThreshold = 0.04f;

// Computes the new spectral difference stats and pushes them into the passed
// symmetric matrix buffer.
void UpdateSpectralDifferenceStats(
    rtc::ArrayView<const float, kNumBands> new_spectral_coeffs,
    const RingBuffer<float, kNumBands, kSpectralCoeffsHistorySize>& ring_buf,
    SymmetricMatrixBuffer<float, kSpectralCoeffsHistorySize>* sym_matrix_buf) {
  RTC_DCHECK(sym_matrix_buf);
  // Compute the new spectral distance stats.
  std::array<float, kSpectralCoeffsHistorySize - 1> distances;
  for (size_t i = 0; i < kSpectralCoeffsHistorySize - 1; ++i) {
    const size_t delay = i + 1;
    auto old_spectral_coeffs = ring_buf.GetArrayView(delay);
    distances[i] = 0.f;
    for (size_t k = 0; k < kNumBands; ++k) {
      const float c = new_spectral_coeffs[k] - old_spectral_coeffs[k];
      distances[i] += c * c;
    }
  }
  // Push the new spectral distance stats into the symmetric matrix buffer.
  sym_matrix_buf->Push(distances);
}

}  // namespace

SpectralFeaturesView::SpectralFeaturesView(
    rtc::ArrayView<float, kNumBands - kNumLowerBands> coeffs,
    rtc::ArrayView<float, kNumLowerBands> average,
    rtc::ArrayView<float, kNumLowerBands> first_derivative,
    rtc::ArrayView<float, kNumLowerBands> second_derivative,
    rtc::ArrayView<float, kNumLowerBands> bands_cross_corr,
    float* variability)
    : coeffs(coeffs),
      average(average),
      first_derivative(first_derivative),
      second_derivative(second_derivative),
      bands_cross_corr(bands_cross_corr),
      variability(variability) {}

SpectralFeaturesView::SpectralFeaturesView(const SpectralFeaturesView&) =
    default;
SpectralFeaturesView::~SpectralFeaturesView() = default;

SpectralFeaturesExtractor::SpectralFeaturesExtractor()
    : fft_(),
      reference_frame_fft_(kFftSize20ms24kHz),
      lagged_frame_fft_(kFftSize20ms24kHz),
      dct_table_(ComputeDctTable()) {}

SpectralFeaturesExtractor::~SpectralFeaturesExtractor() = default;

void SpectralFeaturesExtractor::Reset() {
  spectral_coeffs_ring_buf_.Reset();
  spectral_diffs_buf_.Reset();
}

bool SpectralFeaturesExtractor::CheckSilenceComputeFeatures(
    rtc::ArrayView<const float, kFrameSize20ms24kHz> reference_frame,
    rtc::ArrayView<const float, kFrameSize20ms24kHz> lagged_frame,
    SpectralFeaturesView spectral_features) {
  // Analyze reference frame.
  fft_.WindowedFft(reference_frame, reference_frame_fft_);
  band_features_extractor_.ComputeBandEnergies(
      reference_frame_fft_,
      {reference_frame_bands_energy_.data(), kOpusBands24kHz});
  // Check if the reference frame has silence.
  const float tot_energy =
      std::accumulate(reference_frame_bands_energy_.begin(),
                      reference_frame_bands_energy_.end(), 0.f);
  if (tot_energy < kSilenceThreshold)
    return true;
  // Analyze lagged frame.
  fft_.WindowedFft(lagged_frame, lagged_frame_fft_);
  band_features_extractor_.ComputeBandEnergies(
      lagged_frame_fft_, {lagged_frame_bands_energy_.data(), kOpusBands24kHz});
  // Log of the band energies for the reference frame.
  std::array<float, kNumBands> log_bands_energy;
  ComputeLogBandEnergiesCoefficients(reference_frame_bands_energy_,
                                     log_bands_energy);
  // Decorrelate band-wise log energy coefficients via DCT.
  std::array<float, kNumBands> log_bands_energy_decorrelated;
  ComputeDct(log_bands_energy, dct_table_, log_bands_energy_decorrelated);
  // Normalize (based on training set stats).
  log_bands_energy_decorrelated[0] -= 12;
  log_bands_energy_decorrelated[1] -= 4;
  // Update the ring buffer and the spectral difference stats.
  spectral_coeffs_ring_buf_.Push(log_bands_energy_decorrelated);
  UpdateSpectralDifferenceStats(log_bands_energy_decorrelated,
                                spectral_coeffs_ring_buf_,
                                &spectral_diffs_buf_);
  // Write the higher bands spectral coefficients.
  auto coeffs_src = spectral_coeffs_ring_buf_.GetArrayView(0);
  RTC_DCHECK_EQ(coeffs_src.size() - kNumLowerBands,
                spectral_features.coeffs.size());
  std::copy(coeffs_src.begin() + kNumLowerBands, coeffs_src.end(),
            spectral_features.coeffs.begin());
  // Compute and write remaining features.
  ComputeAvgAndDerivatives(spectral_features.average,
                           spectral_features.first_derivative,
                           spectral_features.second_derivative);
  ComputeCrossCorrelation(spectral_features.bands_cross_corr);
  RTC_DCHECK(spectral_features.variability);
  *(spectral_features.variability) = ComputeVariability();
  return false;
}

void SpectralFeaturesExtractor::ComputeAvgAndDerivatives(
    rtc::ArrayView<float, kNumLowerBands> average,
    rtc::ArrayView<float, kNumLowerBands> first_derivative,
    rtc::ArrayView<float, kNumLowerBands> second_derivative) {
  auto curr = spectral_coeffs_ring_buf_.GetArrayView(0);
  auto prev1 = spectral_coeffs_ring_buf_.GetArrayView(1);
  auto prev2 = spectral_coeffs_ring_buf_.GetArrayView(2);
  RTC_DCHECK_EQ(average.size(), first_derivative.size());
  RTC_DCHECK_EQ(first_derivative.size(), second_derivative.size());
  RTC_DCHECK_LE(average.size(), curr.size());
  for (size_t i = 0; i < average.size(); ++i) {
    // Average, kernel: [1, 1, 1].
    average[i] = curr[i] + prev1[i] + prev2[i];
    // First derivative, kernel: [1, 0, - 1].
    first_derivative[i] = curr[i] - prev2[i];
    // Second derivative, Laplacian kernel: [1, -2, 1].
    second_derivative[i] = curr[i] - 2 * prev1[i] + prev2[i];
  }
}

void SpectralFeaturesExtractor::ComputeCrossCorrelation(
    rtc::ArrayView<float, kNumLowerBands> bands_cross_corr) {
  band_features_extractor_.ComputeSpectralCrossCorrelation(
      reference_frame_fft_, lagged_frame_fft_,
      {bands_cross_corr_.data(), kOpusBands24kHz});
  // Normalize.
  for (size_t i = 0; i < kOpusBands24kHz; ++i) {
    bands_cross_corr_[i] =
        bands_cross_corr_[i] /
        std::sqrt(0.001f + reference_frame_bands_energy_[i] *
                               lagged_frame_bands_energy_[i]);
  }
  // Decorrelate.
  ComputeDct(bands_cross_corr_, dct_table_, bands_cross_corr);
  // Normalize (based on training set stats).
  bands_cross_corr[0] -= 1.3f;
  bands_cross_corr[1] -= 0.9f;
}

float SpectralFeaturesExtractor::ComputeVariability() {
  // Compute spectral variability score.
  float spec_variability = 0.f;
  for (size_t delay1 = 0; delay1 < kSpectralCoeffsHistorySize; ++delay1) {
    float min_dist = std::numeric_limits<float>::max();
    for (size_t delay2 = 0; delay2 < kSpectralCoeffsHistorySize; ++delay2) {
      if (delay1 == delay2)  // The distance would be 0.
        continue;
      min_dist =
          std::min(min_dist, spectral_diffs_buf_.GetValue(delay1, delay2));
    }
    spec_variability += min_dist;
  }
  // Normalize (based on training set stats).
  return spec_variability / kSpectralCoeffsHistorySize - 2.1f;
}

}  // namespace rnn_vad
}  // namespace webrtc
