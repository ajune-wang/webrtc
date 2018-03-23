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

#include <array>
#include <complex>

#include "api/array_view.h"
#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/ring_buffer.h"
#include "common_audio/rnn_vad/rnn_vad_fft.h"
#include "common_audio/rnn_vad/symmetric_matrix_buffer.h"

namespace webrtc {
namespace rnn_vad {

// Compute FFT boundary indexes corresponding to Opus bands.
std::array<size_t, kNumOpusBands> ComputeOpusBandBoundaries(
    const size_t sample_rate,
    const size_t frame_size);

// Given an array of FFT coefficients and a vector of band boundary indexes,
// compute band energy coefficients.
void ComputeBandEnergies(rtc::ArrayView<const std::complex<float>> fft_coeffs,
                         rtc::ArrayView<const size_t> band_boundaries,
                         rtc::ArrayView<float> band_energies);

// Compute DCT for |in| given a pre-computed DCT table and scaling factor.
// In-place computation is not allowed and |out| can be smaller than |in| in
// order to only compute the first DCT coefficients.
void ComputeDct(rtc::ArrayView<const float> in,
                rtc::ArrayView<const float> dct_table,
                const float dct_scaling_factor_,
                rtc::ArrayView<float> out);

using RingBufferType =
    RingBuffer<float, kNumOpusBands, kSpectralCoeffsHistorySize>;
using SymmetricMatrixBufferType =
    SymmetricMatrixBuffer<float, kSpectralCoeffsHistorySize>;

// Push a vector of spectral coefficients in a ring buffer and update the buffer
// of spectral coefficients distances.
void PushSpectralCoeffsUpdSpectralDifferences(
    rtc::ArrayView<const float, kNumOpusBands> spectral_coeffs,
    RingBufferType* ring_buf,
    SymmetricMatrixBufferType* sym_matrix_buf);

// Class to compute spectral features.
class SpectralFeaturesExtractor {
 public:
  SpectralFeaturesExtractor();
  SpectralFeaturesExtractor(const SpectralFeaturesExtractor&) = delete;
  SpectralFeaturesExtractor& operator=(const SpectralFeaturesExtractor&) =
      delete;
  ~SpectralFeaturesExtractor();
  // Analyze reference and lagged frames used to compute spectral features.
  // Return true if silence is detected.
  bool AnalyzeCheckSilence(rtc::ArrayView<const float> reference_frame,
                           rtc::ArrayView<const float> lagged_frame);
  void ComputeAvgAndDeltas(
      rtc::ArrayView<float, kNumOpusBands> avg,
      rtc::ArrayView<float, kNumBandEnergyCoeffDeltas> delta1,
      rtc::ArrayView<float, kNumBandEnergyCoeffDeltas> delta2) const;
  void ComputeCorrelation(rtc::ArrayView<float, kNumBandCorrCoeffs> dst) const;
  float ComputeSpectralVariability() const;

 private:
  RnnVadFft fft_;
  std::array<std::complex<float>, kFftNumCoeffs20ms12kHz> reference_frame_fft_;
  std::array<std::complex<float>, kFftNumCoeffs20ms12kHz> lagged_frame_fft_;
  std::array<float, kNumOpusBands> reference_frame_energy_coeffs_;
  std::array<float, kNumOpusBands> lagged_frame_energy_coeffs_;
  const std::array<size_t, kNumOpusBands> band_boundaries_;
  const std::array<float, kNumOpusBands * kNumOpusBands> dct_table_;
  const float dct_scaling_factor_;
  RingBufferType spectral_coeffs_ring_buf_;
  SymmetricMatrixBufferType spectral_diffs_buf_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_SPECTRAL_FEATURES_H_
