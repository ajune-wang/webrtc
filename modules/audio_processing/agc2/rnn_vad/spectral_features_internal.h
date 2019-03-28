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

// At a sample rate of 24 kHz, the last 3 Opus bands are beyond the Nyquist
// frequency. However, band #19 gets the contributions from band #18 because
// of the symmetric triangular filter with peak response at 12 kHz.
constexpr size_t kOpusBands24kHz = 20;
static_assert(kOpusBands24kHz < kNumBands,
              "The number of bands at 24 kHz must be less than those defined "
              "in the Opus scale at 48 kHz.");

// Class to compute band-wise spectral features in the Opus perceptual scale
// for 20 ms frames sampled at 24 kHz. The analysis methods apply triangular
// filters with peak response at the each band boundary.
class BandFeaturesExtractor {
 public:
  // Ctor.
  BandFeaturesExtractor();
  BandFeaturesExtractor(const BandFeaturesExtractor&) = delete;
  BandFeaturesExtractor& operator=(const BandFeaturesExtractor&) = delete;
  ~BandFeaturesExtractor();

  // Computes the band-wise energies.
  void ComputeBandEnergies(
      rtc::ArrayView<const std::complex<float>> fft,
      rtc::ArrayView<float, kOpusBands24kHz> bands_energy) const;

  // Computes the band-wise spectral cross-correlations.
  void ComputeSpectralCrossCorrelation(
      rtc::ArrayView<const std::complex<float>> x_fft,
      rtc::ArrayView<const std::complex<float>> y_fft,
      rtc::ArrayView<float, kOpusBands24kHz> cross_corr) const;

 private:
  const std::vector<float> weights_;  // Weights for each Fourier coefficient.
};

// Computes log band energy coefficients.
void ComputeLogBandEnergiesCoefficients(
    rtc::ArrayView<const float, kNumBands> bands_energy,
    rtc::ArrayView<float, kNumBands> log_bands_energy);

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
