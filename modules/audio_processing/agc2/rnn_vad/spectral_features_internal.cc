/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/spectral_features_internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// DCT scaling factor.
static_assert(
    kNumBands == 22,
    "kNumBands changed! Please update the value of kDctScalingFactor");
constexpr float kDctScalingFactor = 0.301511345f;  // sqrt(2 / kNumBands)

std::array<size_t, kNumBands> ComputeBandBoundaries(size_t sample_rate_hz,
                                                    size_t frame_size) {
  std::array<size_t, kNumBands> indexes;
  for (size_t i = 0; i < kNumBands; ++i) {
    indexes[i] = kBandFrequencyBoundaries[i] * frame_size / sample_rate_hz;
  }
  return indexes;
}

std::vector<std::vector<float>> ComputeTriangularFiltersWeights(
    const std::array<size_t, kNumBands>& band_boundaries,
    size_t frame_size) {
  // Since the triangular filters are symmetric, the weights for the last band
  // are not stored.
  std::vector<std::vector<float>> weights(kNumBands - 1);
  const size_t fft_size = frame_size / 2 + 1;
  // Define the weights for each triangular filter.
  for (size_t i = 0; i < kNumBands - 1; ++i) {
    // Compute the interval [j0:j1), which is the interval of FFT coefficient
    // indexes for the current band.
    const size_t j0 = band_boundaries[i];
    RTC_DCHECK_GT(band_boundaries[i + 1], band_boundaries[i]);
    const size_t band_size = band_boundaries[i + 1] - band_boundaries[i];
    const size_t j1 = std::min(fft_size, j0 + band_size);
    // Depending on sample rate, the highest bands can have no FFT coefficients.
    // The weight vector in this case is empty.
    if (j0 >= j1) {
      weights[i].resize(0);
      continue;
    }
    // The band weights correspond to a triangular band with peak response at
    // the band boundary.
    weights[i].resize(j1 - j0);
    for (size_t j = 0; j < weights[i].size(); ++j) {
      weights[i][j] = static_cast<float>(j) / band_size;
    }
  }
  return weights;
}

}  // namespace

TriangularFilters::TriangularFilters(size_t sample_rate_hz, size_t frame_size)
    : band_boundaries_(ComputeBandBoundaries(sample_rate_hz, frame_size)),
      weights_(ComputeTriangularFiltersWeights(band_boundaries_, frame_size)) {}

TriangularFilters::~TriangularFilters() = default;

rtc::ArrayView<const size_t, kNumBands> TriangularFilters::GetBandBoundaries()
    const {
  return band_boundaries_;
}

rtc::ArrayView<const float> TriangularFilters::GetBandWeights(
    size_t band_index) const {
  RTC_DCHECK_LT(band_index, weights_.size());
  return weights_[band_index];
}

void ComputeBandEnergies(rtc::ArrayView<const std::complex<float>> fft_coeffs,
                         const TriangularFilters& triangular_filters,
                         rtc::ArrayView<float, kNumBands> band_energies) {
  auto band_boundaries = triangular_filters.GetBandBoundaries();
  static_assert(band_boundaries.size() == band_energies.size(), "");
  std::fill(band_energies.begin(), band_energies.end(), 0.f);
  for (size_t i = 0; i < kNumBands - 1; ++i) {
    auto weights = triangular_filters.GetBandWeights(i);
    // Stop the iteration when coming across the first empty band.
    if (weights.size() == 0) {
      break;
    }
    // Compute the band energies using the current triangular filter.
    RTC_DCHECK_EQ(0.f, band_energies[i + 1]);
    for (size_t j = 0; j < weights.size(); ++j) {
      size_t k = band_boundaries[i] + j;
      RTC_DCHECK_LT(k, fft_coeffs.size());
      float coefficient = std::norm(fft_coeffs[k]);
      band_energies[i] += (1.f - weights[j]) * coefficient;
      band_energies[i + 1] += weights[j] * coefficient;
    }
  }
  // The first and the last bands in the loop above only got half contribution.
  band_energies[0] *= 2.f;
  band_energies[band_energies.size() - 1] *= 2.f;
}

void ComputeLogBandEnergiesCoefficients(
    rtc::ArrayView<const float, kNumBands> band_energy_coeffs,
    rtc::ArrayView<float, kNumBands> log_band_energy_coeffs) {
  float log_max = -2.f;
  float follow = -2.f;
  for (size_t i = 0; i < band_energy_coeffs.size(); ++i) {
    log_band_energy_coeffs[i] = std::log10(1e-2f + band_energy_coeffs[i]);
    // Smoothing across frequency bands.
    log_band_energy_coeffs[i] = std::max(
        log_max - 7.f, std::max(follow - 1.5f, log_band_energy_coeffs[i]));
    log_max = std::max(log_max, log_band_energy_coeffs[i]);
    follow = std::max(follow - 1.5f, log_band_energy_coeffs[i]);
  }
}

std::array<float, kNumBands * kNumBands> ComputeDctTable() {
  std::array<float, kNumBands * kNumBands> dct_table;
  const double k = std::sqrt(0.5);
  for (size_t i = 0; i < kNumBands; ++i) {
    for (size_t j = 0; j < kNumBands; ++j)
      dct_table[i * kNumBands + j] = std::cos((i + 0.5) * j * kPi / kNumBands);
    dct_table[i * kNumBands] *= k;
  }
  return dct_table;
}

void ComputeDct(rtc::ArrayView<const float, kNumBands> in,
                rtc::ArrayView<const float, kNumBands * kNumBands> dct_table,
                rtc::ArrayView<float> out) {
  RTC_DCHECK_NE(in.data(), out.data()) << "In-place DCT is not supported.";
  RTC_DCHECK_LE(1, out.size());
  RTC_DCHECK_LE(out.size(), in.size());
  std::fill(out.begin(), out.end(), 0.f);
  for (size_t i = 0; i < out.size(); ++i) {
    for (size_t j = 0; j < in.size(); ++j) {
      out[i] += in[j] * dct_table[j * in.size() + i];
    }
    out[i] *= kDctScalingFactor;
  }
}

}  // namespace rnn_vad
}  // namespace webrtc
