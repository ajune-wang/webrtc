/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SPECTRAL_FEATURES_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SPECTRAL_FEATURES_H_

#include <array>
#include <complex>
#include <cstddef>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/ring_buffer.h"
#include "modules/audio_processing/agc2/rnn_vad/spectral_features_internal.h"
#include "modules/audio_processing/agc2/rnn_vad/symmetric_matrix_buffer.h"
#include "modules/audio_processing/utility/pffft_wrapper.h"

namespace webrtc {
namespace rnn_vad {

// View on spectral features.
class SpectralFeaturesView {
 public:
  SpectralFeaturesView(rtc::ArrayView<float, kNumBands - kNumLowerBands> coeffs,
                       rtc::ArrayView<float, kNumLowerBands> average,
                       rtc::ArrayView<float, kNumLowerBands> first_derivative,
                       rtc::ArrayView<float, kNumLowerBands> second_derivative,
                       rtc::ArrayView<float, kNumLowerBands> bands_cross_corr,
                       float* variability);
  SpectralFeaturesView(const SpectralFeaturesView&);
  ~SpectralFeaturesView();
  // Higher bands spectral coefficients.
  const rtc::ArrayView<float, kNumBands - kNumLowerBands> coeffs;
  // Average and first and second derivative over time for the lower bands.
  const rtc::ArrayView<float, kNumLowerBands> average;
  const rtc::ArrayView<float, kNumLowerBands> first_derivative;
  const rtc::ArrayView<float, kNumLowerBands> second_derivative;
  // Spectral cross-correlation for the lower bands.
  const rtc::ArrayView<float, kNumLowerBands> bands_cross_corr;
  // Spectral variability score.
  float* const variability;
};

// Class to compute spectral features.
class SpectralFeaturesExtractor {
 public:
  SpectralFeaturesExtractor();
  SpectralFeaturesExtractor(const SpectralFeaturesExtractor&) = delete;
  SpectralFeaturesExtractor& operator=(const SpectralFeaturesExtractor&) =
      delete;
  ~SpectralFeaturesExtractor();
  // Resets the internal state of the feature extractor.
  void Reset();
  // Analyzes a pair of reference and lagged frames from the pitch buffer,
  // detects silence and computes features. If silence is detected, the output
  // is neither computed nor written.
  bool CheckSilenceComputeFeatures(
      rtc::ArrayView<const float, kFrameSize20ms24kHz> reference_frame,
      rtc::ArrayView<const float, kFrameSize20ms24kHz> lagged_frame,
      SpectralFeaturesView spectral_features);

 private:
  void ComputeAvgAndDerivatives(
      rtc::ArrayView<float, kNumLowerBands> average,
      rtc::ArrayView<float, kNumLowerBands> first_derivative,
      rtc::ArrayView<float, kNumLowerBands> second_derivative);
  void ComputeCrossCorrelation(
      rtc::ArrayView<float, kNumLowerBands> bands_cross_corr);
  float ComputeVariability();

  const std::array<float, kFrameSize20ms24kHz / 2> half_window_;
  Pffft fft_;
  std::unique_ptr<Pffft::FloatBuffer> fft_buffer_;
  std::unique_ptr<Pffft::FloatBuffer> reference_frame_fft_;
  std::unique_ptr<Pffft::FloatBuffer> lagged_frame_fft_;
  BandFeaturesExtractor band_features_extractor_;
  std::array<float, kNumBands> reference_frame_bands_energy_{};
  std::array<float, kNumBands> lagged_frame_bands_energy_{};
  std::array<float, kNumBands> bands_cross_corr_{};
  const std::array<float, kNumBands * kNumBands> dct_table_;
  RingBuffer<float, kNumBands, kSpectralCoeffsHistorySize>
      spectral_coeffs_ring_buf_;
  SymmetricMatrixBuffer<float, kSpectralCoeffsHistorySize> spectral_diffs_buf_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SPECTRAL_FEATURES_H_
