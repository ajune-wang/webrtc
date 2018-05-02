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
#include <cstring>
#include <numeric>

#include "modules/audio_processing/agc2/rnn_vad/spectral_features_internal.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// Pushes a vector of spectral coefficients in a ring buffer and update the
// buffer of spectral coefficients distances.
void PushSpectralCoeffsUpdSpectralDifferences(
    rtc::ArrayView<const float, kNumBands> new_spectral_coeffs,
    RingBuffer<float, kNumBands, kSpectralCoeffsHistorySize>* ring_buf,
    SymmetricMatrixBuffer<float, kSpectralCoeffsHistorySize>* sym_matrix_buf) {
  RTC_DCHECK(ring_buf);
  RTC_DCHECK(sym_matrix_buf);
  // Push the spectral coefficients into the ring buffer.
  ring_buf->Push(new_spectral_coeffs);
  // Compute the new spectral distance coefficients.
  std::array<float, kSpectralCoeffsHistorySize - 1> distances;
  for (size_t i = 0; i < kSpectralCoeffsHistorySize - 1; ++i) {
    const size_t delay = i + 1;
    auto old_spectral_coeffs = ring_buf->GetArrayView(delay);
    distances[i] = 0.f;
    for (size_t k = 0; k < kNumBands; ++k) {
      const float c = new_spectral_coeffs[k] - old_spectral_coeffs[k];
      distances[i] += c * c;
    }
  }
  // Push the new spectral distance coefficients into the symmetric matrix
  // buffer.
  sym_matrix_buf->Push({distances.data(), distances.size()});
}

}  // namespace

SpectralFeaturesExtractor::SpectralFeaturesExtractor()
    : fft_(),
      band_boundaries_(
          ComputeBandBoundaryIndexes(kSampleRate24kHz, kFrameSize20ms24kHz)),
      dct_table_(ComputeDctTable()) {}

SpectralFeaturesExtractor::~SpectralFeaturesExtractor() = default;

void SpectralFeaturesExtractor::Reset() {
  spectral_coeffs_ring_buf_.Reset();
  spectral_diffs_buf_.Reset();
}

bool SpectralFeaturesExtractor::CheckSilenceComputeFeatures(
    rtc::ArrayView<const float, kFrameSize20ms24kHz> reference_frame,
    rtc::ArrayView<const float, kFrameSize20ms24kHz> lagged_frame,
    rtc::ArrayView<float, kNumBands - kNumLowerBands> coeffs,
    rtc::ArrayView<float, kNumLowerBands> average,
    rtc::ArrayView<float, kNumLowerBands> first_derivative,
    rtc::ArrayView<float, kNumLowerBands> second_derivative,
    rtc::ArrayView<float, kNumLowerBands> cross_correlations,
    float* variability) {
  // Analyze reference frame.
  fft_.ForwardFft(reference_frame, reference_frame_fft_);
  ComputeBandEnergies(reference_frame_fft_,
                      {band_boundaries_.data(), band_boundaries_.size()},
                      {reference_frame_energy_coeffs_.data(),
                       reference_frame_energy_coeffs_.size()});
  // Check if the reference frame has silence.
  float tot_energy = std::accumulate(reference_frame_energy_coeffs_.begin(),
                                     reference_frame_energy_coeffs_.end(), 0.f);
  if (tot_energy < 0.04f)
    return true;
  // Analyze lagged frame.
  fft_.ForwardFft(lagged_frame, lagged_frame_fft_);
  ComputeBandEnergies(
      lagged_frame_fft_, {band_boundaries_.data(), band_boundaries_.size()},
      {lagged_frame_energy_coeffs_.data(), lagged_frame_energy_coeffs_.size()});
  // Log of the band energies for the reference frame.
  std::array<float, kNumBands> log_band_energy_coeffs;
  ComputeLogBandEnergiesCoefficients(
      {reference_frame_energy_coeffs_.data(),
       reference_frame_energy_coeffs_.size()},
      {log_band_energy_coeffs.data(), log_band_energy_coeffs.size()});
  // Decorrelate band-wise log energy coefficients via DCT.
  std::array<float, kNumBands> log_band_energy_coeffs_decorrelated;
  ComputeDct({log_band_energy_coeffs.data(), log_band_energy_coeffs.size()},
             {dct_table_.data(), dct_table_.size()},
             {log_band_energy_coeffs_decorrelated.data(),
              log_band_energy_coeffs_decorrelated.size()});
  // Normalize.
  log_band_energy_coeffs_decorrelated[0] -= 12;
  log_band_energy_coeffs_decorrelated[1] -= 4;
  // Update the ring buffer and the symmetric matrix with the new spectral
  // features.
  PushSpectralCoeffsUpdSpectralDifferences(
      {log_band_energy_coeffs_decorrelated.data(),
       log_band_energy_coeffs_decorrelated.size()},
      &spectral_coeffs_ring_buf_, &spectral_diffs_buf_);
  // Write the higher bands spectral coefficients.
  auto coeffs_src = spectral_coeffs_ring_buf_.GetArrayView(0);
  std::memcpy(coeffs.data(), coeffs_src.data() + kNumLowerBands,
              coeffs.size() * sizeof(float));
  // Compute and write remaining features.
  ComputeAvgAndDerivatives(average, first_derivative, second_derivative);
  ComputeCrossCorrelation(cross_correlations);
  RTC_DCHECK(variability);
  *variability = ComputeVariability();
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
    rtc::ArrayView<float, kNumLowerBands> cross_correlations) {
  const auto& x = reference_frame_fft_;
  const auto& y = lagged_frame_fft_;
  auto functor = [x, y](const size_t freq_bin_index) -> float {
    return (x[freq_bin_index].real() * y[freq_bin_index].real() +
            x[freq_bin_index].imag() * y[freq_bin_index].imag());
  };
  std::array<float, kNumBands> cross_corr_coeffs;
  constexpr size_t kNumFftPoints = kFrameSize20ms24kHz / 2 + 1;
  ComputeBandCoefficients(
      functor, {band_boundaries_.data(), band_boundaries_.size()},
      kNumFftPoints - 1, {cross_corr_coeffs.data(), cross_corr_coeffs.size()});
  // Normalize.
  for (size_t i = 0; i < cross_corr_coeffs.size(); ++i) {
    cross_corr_coeffs[i] =
        cross_corr_coeffs[i] /
        std::sqrt(0.001f + reference_frame_energy_coeffs_[i] *
                               lagged_frame_energy_coeffs_[i]);
  }
  // Decorrelate.
  ComputeDct({cross_corr_coeffs.data(), cross_corr_coeffs.size()},
             {dct_table_.data(), dct_table_.size()},
             {cross_correlations.data(), cross_correlations.size()});
  // Normalize.
  cross_correlations[0] -= 1.3;
  cross_correlations[1] -= 0.9;
}

float SpectralFeaturesExtractor::ComputeVariability() {
  // Compute spectral variability score.
  float spec_variability = 0.f;
  for (size_t delay1 = 0; delay1 < kSpectralCoeffsHistorySize; ++delay1) {
    float min_dist = 1e15f;
    for (size_t delay2 = 0; delay2 < kSpectralCoeffsHistorySize; ++delay2) {
      if (delay1 == delay2)  // The distance would be 0.
        continue;
      min_dist =
          std::min(min_dist, spectral_diffs_buf_.GetValue(delay1, delay2));
    }
    spec_variability += min_dist;
  }
  return spec_variability / kSpectralCoeffsHistorySize - 2.1f;
}

}  // namespace rnn_vad
}  // namespace webrtc
