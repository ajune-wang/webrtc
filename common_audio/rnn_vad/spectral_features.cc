/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/spectral_features.h"

#include <algorithm>
#include <functional>
#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {

// Helper function that iterates through frequency bands and computes
// coefficients for triangular bands with peak response at each band boundary.
void ComputeBandCoefficients(
    std::function<float(const size_t)> functor,
    rtc::ArrayView<const size_t, kNumBands> band_boundaries,
    const size_t max_freq_bin_index,
    rtc::ArrayView<float, kNumBands> coefficients) {
  for (auto& v : coefficients)
    v = 0.f;
  for (size_t i = 0; i < coefficients.size() - 1; ++i) {
    RTC_DCHECK_EQ(0.f, coefficients[i + 1]);
    RTC_DCHECK_GT(band_boundaries[i + 1], band_boundaries[i]);
    const size_t first_freq_bin = band_boundaries[i];
    const size_t last_freq_bin =
        std::min(max_freq_bin_index, first_freq_bin + band_boundaries[i + 1] -
                                         band_boundaries[i] - 1);
    if (first_freq_bin >= last_freq_bin)
      break;
    const size_t band_size = last_freq_bin - first_freq_bin + 1;
    // Compute the band coefficient using a triangular band with peak response
    // at the band boundary.
    for (size_t j = first_freq_bin; j <= last_freq_bin; ++j) {
      const float w = static_cast<float>(j - first_freq_bin) / band_size;
      const float coefficient = functor(j);
      coefficients[i] += (1.f - w) * coefficient;
      coefficients[i + 1] += w * coefficient;
    }
  }
  // Fix the first and the last bands (they only got half contribution).
  coefficients[0] *= 2.f;
  coefficients[coefficients.size() - 1] *= 2.f;
  // TODO(alessiob): Check if replacing the line above with the one below leads
  // to better performance.
  // coefficients[i] *= 2.f;
}

std::array<size_t, kNumBands> ComputeBandBoundaryIndexes(
    const size_t sample_rate,
    const size_t frame_size) {
  std::array<size_t, kNumBands> indexes;
  for (size_t i = 0; i < kNumBands; ++i)
    indexes[i] = kBandFrequencyBoundaries[i] * frame_size / sample_rate;
  return indexes;
}

void ComputeBandEnergies(
    rtc::ArrayView<const std::complex<float>> fft_coeffs,
    rtc::ArrayView<const size_t, kNumBands> band_boundaries,
    rtc::ArrayView<float, kNumBands> band_energies) {
  RTC_DCHECK_EQ(band_boundaries.size(), band_energies.size());
  auto functor = [fft_coeffs](const size_t freq_bin_index) {
    return std::norm(fft_coeffs[freq_bin_index]);
  };
  ComputeBandCoefficients(functor, band_boundaries, fft_coeffs.size() - 1,
                          band_energies);
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

void ComputeLogBandEnergiesCoefficients(
    rtc::ArrayView<const float, kNumBands> band_energy_coeffs,
    rtc::ArrayView<float, kNumBands> log_band_energy_coeffs) {
  // TODO(alessiob): Describe smoothing with log_max and follow.
  float log_max = -2.f;
  float follow = -2.f;
  for (size_t i = 0; i < band_energy_coeffs.size(); ++i) {
    log_band_energy_coeffs[i] = std::log10(1e-2f + band_energy_coeffs[i]);
    log_band_energy_coeffs[i] = std::max(
        log_max - 7.f, std::max(follow - 1.5f, log_band_energy_coeffs[i]));
    log_max = std::max(log_max, log_band_energy_coeffs[i]);
    follow = std::max(follow - 1.5f, log_band_energy_coeffs[i]);
  }
}

// TODO(alessiob): Maybe faster if |out| size known at compile time even if all
// the DCT coefficients are computed.
void ComputeDct(rtc::ArrayView<const float, kNumBands> in,
                rtc::ArrayView<const float, kNumBands * kNumBands> dct_table,
                const float dct_scaling_factor_,
                rtc::ArrayView<float> out) {
  RTC_DCHECK_NE(in.data(), out.data()) << "In-place DCT is not supported.";
  RTC_DCHECK_LE(1, out.size());
  RTC_DCHECK_LE(out.size(), in.size());
  for (size_t i = 0; i < out.size(); ++i) {
    out[i] = 0.f;
    for (size_t j = 0; j < in.size(); ++j)
      out[i] += in[j] * dct_table[j * in.size() + i];
    out[i] *= dct_scaling_factor_;
  }
}

bool IsSilence(rtc::ArrayView<const float, kNumBands> band_energy_coeffs) {
  const float tot_energy = std::accumulate(band_energy_coeffs.begin(),
                                           band_energy_coeffs.end(), 0.f);
  return tot_energy < 0.04f;
}

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

}  // namespace rnn_vad
}  // namespace webrtc
