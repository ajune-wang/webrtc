/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SPECTRAL_FEATURES_INTERNAL_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SPECTRAL_FEATURES_INTERNAL_H_

#include <stddef.h>
#include <array>
#include <complex>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"

namespace webrtc {
namespace rnn_vad {

// Overlapping triangular filters weights.
class TriangularFilters {
 public:
  TriangularFilters(size_t sample_rate_hz, size_t frame_size);
  TriangularFilters(const TriangularFilters&) = delete;
  TriangularFilters& operator=(const TriangularFilters&) = delete;
  ~TriangularFilters();

  // Returns the indexes of the first FFT coefficient for each triangular
  // filter.
  rtc::ArrayView<const size_t, kNumBands> GetBandBoundaries() const;

  // Returns the weights for the FFT coefficient of the given band.
  // The band weights correspond to a triangular band with peak response at
  // the band boundary.
  // Since the triangular filters are symmetric around each band boundary, the
  // weights for the last band are not defined - i.e., it must hold
  // |band_index| < kNumBands - 1.
  // When the returned view is empty, it means that there are no available FFT
  // coefficients for that band (because the Nyquist frequency is too low).
  rtc::ArrayView<const float> GetBandWeights(size_t band_index) const;

 private:
  const std::array<size_t, kNumBands> band_boundaries_;
  const std::vector<std::vector<float>> weights_;
};

// Given an array of FFT coefficients and a vector of band boundary indexes,
// computes band energy coefficients.
void ComputeBandEnergies(rtc::ArrayView<const std::complex<float>> fft_coeffs,
                         const TriangularFilters& triangular_filters,
                         rtc::ArrayView<float, kNumBands> band_energies);

// Computes log band energy coefficients.
void ComputeLogBandEnergiesCoefficients(
    rtc::ArrayView<const float, kNumBands> band_energy_coeffs,
    rtc::ArrayView<float, kNumBands> log_band_energy_coeffs);

// Creates a DCT table for arrays having size equal to |kNumBands|.
std::array<float, kNumBands * kNumBands> ComputeDctTable();

// Computes DCT for |in| given a pre-computed DCT table. In-place computation is
// not allowed and |out| can be smaller than |in| in order to only compute the
// first DCT coefficients.
void ComputeDct(rtc::ArrayView<const float, kNumBands> in,
                rtc::ArrayView<const float, kNumBands * kNumBands> dct_table,
                rtc::ArrayView<float> out);

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SPECTRAL_FEATURES_INTERNAL_H_
