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
namespace {

// Helper function that iterates through frequency bands and computes
// coefficients for triangular bands with peak response at each band boundary.
void ComputeBandCoefficients(std::function<float(const size_t)> functor,
                             rtc::ArrayView<const size_t> band_boundaries,
                             const size_t max_freq_bin_index,
                             rtc::ArrayView<float> coefficients) {
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

// Push a vector of spectral coefficients in a ring buffer and update the buffer
// of spectral coefficients distances.
void PushSpectralCoeffsUpdSpectralDifferences(
    rtc::ArrayView<const float, kNumOpusBands> new_spectral_coeffs,
    RingBuffer<float, kNumOpusBands, kSpectralCoeffsHistorySize>* ring_buf,
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
    for (size_t k = 0; k < kNumOpusBands; ++k) {
      const float c = new_spectral_coeffs[k] - old_spectral_coeffs[k];
      distances[i] += c * c;
    }
  }
  // Push the new spectral distance coefficients into the symmetric matrix
  // buffer.
  sym_matrix_buf->Push({distances.data(), distances.size()});
}

}  // namespace

std::array<size_t, kNumOpusBands> ComputeOpusBandBoundaries(
    const size_t sample_rate,
    const size_t frame_size) {
  std::array<size_t, kNumOpusBands> indexes;
  for (size_t i = 0; i < kNumOpusBands; ++i)
    indexes[i] = kOpusBandsFrequencies[i] * frame_size / sample_rate;
  return indexes;
}

void ComputeBandEnergies(rtc::ArrayView<const std::complex<float>> fft_coeffs,
                         rtc::ArrayView<const size_t> band_boundaries,
                         rtc::ArrayView<float> band_energies) {
  RTC_DCHECK_EQ(band_boundaries.size(), band_energies.size());
  const float scaling = 1.f / (2 * fft_coeffs.size() - 2);
  auto functor = [scaling, fft_coeffs](const size_t freq_bin_index) {
    return std::norm(fft_coeffs[freq_bin_index] * scaling);
  };
  ComputeBandCoefficients(functor, band_boundaries, fft_coeffs.size() - 1,
                          band_energies);
}

std::array<float, kNumOpusBands * kNumOpusBands> ComputeDctTable() {
  std::array<float, kNumOpusBands * kNumOpusBands> dct_table;
  const double k = std::sqrt(0.5);
  for (size_t i = 0; i < kNumOpusBands; ++i) {
    for (size_t j = 0; j < kNumOpusBands; ++j)
      dct_table[i * kNumOpusBands + j] =
          std::cos((i + 0.5) * j * kPi / kNumOpusBands);
    dct_table[i * kNumOpusBands] *= k;
  }
  return dct_table;
}

void ComputeLogBandEnergiesCoefficients(
    rtc::ArrayView<const float> band_energy_coeffs,
    rtc::ArrayView<float> log_band_energy_coeffs) {
  // TODO(alessiob): Describe smoothing with log_max and follow.
  RTC_DCHECK_EQ(band_energy_coeffs.size(), log_band_energy_coeffs.size());
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

void ComputeDct(
    rtc::ArrayView<const float, kNumOpusBands> in,
    rtc::ArrayView<const float, kNumOpusBands * kNumOpusBands> dct_table,
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

SpectralFeaturesExtractor::SpectralFeaturesExtractor()
    : is_silence_(true),
      fft_(kFrameSize20ms24kHz),
      band_boundaries_(
          ComputeOpusBandBoundaries(kSampleRate24kHz, kFftLenght20ms24kHz)),
      dct_table_(ComputeDctTable()),
      dct_scaling_factor_(ComputeDctScalingFactor(kNumOpusBands)) {
  RTC_CHECK_EQ(kFftLenght20ms24kHz, fft_.fft_length());
  RTC_CHECK_EQ(kFftNumCoeffs20ms24kHz, fft_.num_fft_points());
}

SpectralFeaturesExtractor::~SpectralFeaturesExtractor() = default;

bool SpectralFeaturesExtractor::AnalyzeCheckSilence(
    rtc::ArrayView<const float, kFrameSize20ms24kHz> reference_frame,
    rtc::ArrayView<const float, kFrameSize20ms24kHz> lagged_frame) {
  // Check if the reference frame corresponds to silence.
  fft_.ForwardFft(reference_frame);
  fft_.CopyOutput(reference_frame_fft_);
  ComputeBandEnergies(reference_frame_fft_, {band_boundaries_},
                      {reference_frame_energy_coeffs_});
  const float tot_energy =
      std::accumulate(reference_frame_energy_coeffs_.begin(),
                      reference_frame_energy_coeffs_.end(), 0.f);
  is_silence_ = tot_energy < 0.04f;
  if (is_silence_)  // Check if silence.
    return true;
  // Analyze lagged frame.
  fft_.ForwardFft(lagged_frame);
  fft_.CopyOutput(lagged_frame_fft_);
  ComputeBandEnergies({lagged_frame_fft_}, {band_boundaries_},
                      {lagged_frame_energy_coeffs_});
  // Log of the band energies for the reference frame.
  std::array<float, kNumOpusBands> log_band_energy_coeffs;
  ComputeLogBandEnergiesCoefficients({reference_frame_energy_coeffs_},
                                     {log_band_energy_coeffs});
  // Decorrelate band-wise log energy coefficients via DCT.
  std::array<float, kNumOpusBands> log_band_energy_coeffs_decorrelated;
  ComputeDct({log_band_energy_coeffs.data(), log_band_energy_coeffs.size()},
             {dct_table_.data(), dct_table_.size()}, dct_scaling_factor_,
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
  return false;
}

void SpectralFeaturesExtractor::CopySpectralCoefficients(
    rtc::ArrayView<float> dst,
    const size_t offset) const {
  RTC_DCHECK(!is_silence_)
      << "The client code must not compute features when silence is detected";
  auto src = spectral_coeffs_ring_buf_.GetArrayView(0);
  RTC_DCHECK_LE(dst.size(), src.size() - offset);
  std::memcpy(dst.data(), src.data() + offset, dst.size() * sizeof(float));
}

void SpectralFeaturesExtractor::ComputeAvgAndDeltas(
    rtc::ArrayView<float, kNumBandEnergyCoeffDeltas> avg,
    rtc::ArrayView<float, kNumBandEnergyCoeffDeltas> delta1,
    rtc::ArrayView<float, kNumBandEnergyCoeffDeltas> delta2) const {
  RTC_DCHECK(!is_silence_)
      << "The client code must not compute features when silence is detected";
  auto curr = spectral_coeffs_ring_buf_.GetArrayView(0);
  auto prev1 = spectral_coeffs_ring_buf_.GetArrayView(1);
  auto prev2 = spectral_coeffs_ring_buf_.GetArrayView(2);
  RTC_DCHECK_EQ(avg.size(), delta1.size());
  RTC_DCHECK_EQ(delta1.size(), delta2.size());
  RTC_DCHECK_LE(avg.size(), curr.size());
  for (size_t i = 0; i < avg.size(); ++i) {
    // Average, kernel: [1, 1, 1].
    avg[i] = curr[i] + prev1[i] + prev2[i];
    // First derivative, kernel: [1, 0, - 1].
    delta1[i] = curr[i] - prev2[i];
    // Second derivative, Laplacian kernel: [1, -2, 1].
    delta2[i] = curr[i] - 2 * prev1[i] + prev2[i];
  }
}

void SpectralFeaturesExtractor::ComputeCorrelation(
    rtc::ArrayView<float, kNumBandCorrCoeffs> dst) const {
  RTC_DCHECK(!is_silence_)
      << "The client code must not compute features when silence is detected";
  const auto* x = &reference_frame_fft_;
  const auto* y = &lagged_frame_fft_;
  auto functor = [x, y](const size_t freq_bin_index) {
    return ((*x)[freq_bin_index].real() * (*y)[freq_bin_index].real() +
            (*x)[freq_bin_index].imag() * (*y)[freq_bin_index].imag()) *
           kFftLenght20ms24kHz * kFftLenght20ms24kHz;  // Scaling.
  };
  std::array<float, kNumOpusBands> band_corr_coeffs;
  ComputeBandCoefficients(functor, band_boundaries_,
                          reference_frame_fft_.size() - 1, band_corr_coeffs);
  // Normalize.
  for (size_t i = 0; i < band_corr_coeffs.size(); ++i) {
    band_corr_coeffs[i] =
        band_corr_coeffs[i] /
        std::sqrt(0.001f + reference_frame_energy_coeffs_[i] *
                               lagged_frame_energy_coeffs_[i]);
  }
  // Decorrelate.
  ComputeDct({band_corr_coeffs.data(), band_corr_coeffs.size()},
             {dct_table_.data(), dct_table_.size()}, dct_scaling_factor_,
             {dst.data(), dst.size()});
  // Normalize.
  dst[0] -= 1.3;
  dst[1] -= 0.9;
}

float SpectralFeaturesExtractor::ComputeSpectralVariability() const {
  RTC_DCHECK(!is_silence_)
      << "The client code must not compute features when silence is detected";
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
  return spec_variability;
}

}  // namespace rnn_vad
}  // namespace webrtc
