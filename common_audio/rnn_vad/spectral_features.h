/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_SPECTRAL_FEATURES_H_
#define COMMON_AUDIO_RNN_VAD_SPECTRAL_FEATURES_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

#include "api/array_view.h"
#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/ring_buffer.h"
#include "common_audio/rnn_vad/rnn_vad_fft.h"
#include "common_audio/rnn_vad/symmetric_matrix_buffer.h"

namespace webrtc {
namespace rnn_vad {

// Helper function that iterates through frequency bands and computes
// coefficients for triangular bands with peak response at each band boundary.
void ComputeBandCoefficients(
    std::function<float(const size_t)> functor,
    rtc::ArrayView<const size_t, kNumBands> band_boundaries,
    const size_t max_freq_bin_index,
    rtc::ArrayView<float, kNumBands> coefficients);

// Compute FFT boundary indexes corresponding to sub-bands.
std::array<size_t, kNumBands> ComputeBandBoundaryIndexes(
    const size_t sample_rate,
    const size_t frame_size);

// Given an array of FFT coefficients and a vector of band boundary indexes,
// compute band energy coefficients.
void ComputeBandEnergies(
    rtc::ArrayView<const std::complex<float>> fft_coeffs,
    rtc::ArrayView<const size_t, kNumBands> band_boundaries,
    rtc::ArrayView<float, kNumBands> band_energies);

// Compute log band energy coefficients.
void ComputeLogBandEnergiesCoefficients(
    rtc::ArrayView<const float, kNumBands> band_energy_coeffs,
    rtc::ArrayView<float, kNumBands> log_band_energy_coeffs);

// Create a DCT table for arrays having size equal to |kNumBands|.
std::array<float, kNumBands * kNumBands> ComputeDctTable();
// Return the DCT scaling factor for arrays with given size.
inline float ComputeDctScalingFactor(const size_t input_size) {
  return std::sqrt(2.f / input_size);
}
// Compute DCT for |in| given a pre-computed DCT table and scaling factor.
// In-place computation is not allowed and |out| can be smaller than |in| in
// order to only compute the first DCT coefficients.
void ComputeDct(rtc::ArrayView<const float, kNumBands> in,
                rtc::ArrayView<const float, kNumBands * kNumBands> dct_table,
                const float dct_scaling_factor_,
                rtc::ArrayView<float> out);

// Determine if there is silence.
bool IsSilence(rtc::ArrayView<const float, kNumBands> band_energy_coeffs);

using RingBufferType = RingBuffer<float, kNumBands, kSpectralCoeffsHistorySize>;
using SymmetricMatrixBufferType =
    SymmetricMatrixBuffer<float, kSpectralCoeffsHistorySize>;

// Push a vector of spectral coefficients in a ring buffer and update the buffer
// of spectral coefficients distances.
void PushSpectralCoeffsUpdSpectralDifferences(
    rtc::ArrayView<const float, kNumBands> new_spectral_coeffs,
    RingBufferType* ring_buf,
    SymmetricMatrixBufferType* sym_matrix_buf);

// Class to compute spectral features where S is the sample rate and N is both
// the  frame size and the number of computed FFT points.
template <size_t S, size_t N>
class SpectralFeaturesExtractor {
 public:
  static_assert((S & 1) == 0, "The frame size must be an even number.");
  SpectralFeaturesExtractor()
      : is_silence_(true),
        fft_(N),
        band_boundaries_(ComputeBandBoundaryIndexes(S, N)),
        dct_table_(ComputeDctTable()),
        dct_scaling_factor_(ComputeDctScalingFactor(kNumBands)) {}
  SpectralFeaturesExtractor(const SpectralFeaturesExtractor&) = delete;
  SpectralFeaturesExtractor& operator=(const SpectralFeaturesExtractor&) =
      delete;
  ~SpectralFeaturesExtractor() = default;
  // Reset the internal state of the feature extractor.
  void Reset() {
    spectral_coeffs_ring_buf_.Reset();
    spectral_diffs_buf_.Reset();
  }
  // Analyze reference and lagged frames used to compute spectral features.
  // If silence is detected, true is returned and no other methods should be
  // called until AnalyzeCheckSilence() is called again.
  bool AnalyzeCheckSilence(rtc::ArrayView<const float, N> reference_frame,
                           rtc::ArrayView<const float, N> lagged_frame) {
    // Check if the reference frame corresponds to silence.
    fft_.ForwardFft(reference_frame, {reference_frame_fft_});
    ComputeBandEnergies({reference_frame_fft_.data(), num_fft_points_},
                        {band_boundaries_.data(), band_boundaries_.size()},
                        {reference_frame_energy_coeffs_.data(),
                         reference_frame_energy_coeffs_.size()});
    is_silence_ = IsSilence({reference_frame_energy_coeffs_.data(),
                             reference_frame_energy_coeffs_.size()});
    if (is_silence_)  // Check if silence.
      return true;
    // Analyze lagged frame.
    fft_.ForwardFft(lagged_frame, {lagged_frame_fft_});
    ComputeBandEnergies({lagged_frame_fft_.data(), num_fft_points_},
                        {band_boundaries_.data(), band_boundaries_.size()},
                        {lagged_frame_energy_coeffs_.data(),
                         lagged_frame_energy_coeffs_.size()});
    // Log of the band energies for the reference frame.
    std::array<float, kNumBands> log_band_energy_coeffs;
    ComputeLogBandEnergiesCoefficients(
        {reference_frame_energy_coeffs_.data(),
         reference_frame_energy_coeffs_.size()},
        {log_band_energy_coeffs.data(), log_band_energy_coeffs.size()});
    // Decorrelate band-wise log energy coefficients via DCT.
    std::array<float, kNumBands> log_band_energy_coeffs_decorrelated;
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
  // Copies the spectral coefficients starting from that with index equal to
  // |offset|.
  void CopySpectralCoefficients(rtc::ArrayView<float> dst,
                                const size_t offset = 0) const {
    RTC_DCHECK(!is_silence_)
        << "The client code must not compute features when silence is detected";
    auto src = spectral_coeffs_ring_buf_.GetArrayView(0);
    RTC_DCHECK_LE(dst.size(), src.size() - offset);
    std::memcpy(dst.data(), src.data() + offset, dst.size() * sizeof(float));
  }
  // Compute average and first and second derivative of the spectral
  // coefficients.
  void ComputeAvgAndDeltas(
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
  // Compute the spectral correlation scores.
  void ComputeCorrelation(rtc::ArrayView<float, kNumBandCorrCoeffs> dst) const {
    RTC_DCHECK(!is_silence_)
        << "The client code must not compute features when silence is detected";
    const auto* x = &reference_frame_fft_;
    const auto* y = &lagged_frame_fft_;
    auto functor = [x, y](const size_t freq_bin_index) {
      return ((*x)[freq_bin_index].real() * (*y)[freq_bin_index].real() +
              (*x)[freq_bin_index].imag() * (*y)[freq_bin_index].imag());
    };
    std::array<float, kNumBands> band_corr_coeffs;
    ComputeBandCoefficients(functor,
                            {band_boundaries_.data(), band_boundaries_.size()},
                            num_fft_points_ - 1,
                            {band_corr_coeffs.data(), band_corr_coeffs.size()});
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
  // Compute the spectral variability score.
  float ComputeSpectralVariability() const {
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
    return spec_variability / kSpectralCoeffsHistorySize - 2.1f;
  }

 private:
  bool is_silence_;
  static constexpr size_t num_fft_points_ = N / 2 + 1;
  RnnVadFft fft_;
  std::array<std::complex<float>, N> reference_frame_fft_;
  std::array<std::complex<float>, N> lagged_frame_fft_;
  std::array<float, kNumBands> reference_frame_energy_coeffs_;
  std::array<float, kNumBands> lagged_frame_energy_coeffs_;
  const std::array<size_t, kNumBands> band_boundaries_;
  const std::array<float, kNumBands * kNumBands> dct_table_;
  const float dct_scaling_factor_;
  RingBufferType spectral_coeffs_ring_buf_;
  SymmetricMatrixBufferType spectral_diffs_buf_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_SPECTRAL_FEATURES_H_
