/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_FEATURES_EXTRACTION_H_
#define COMMON_AUDIO_RNN_VAD_FEATURES_EXTRACTION_H_

#include <array>
#include <complex>
#include <memory>

#include "api/array_view.h"
#include "common_audio/rnn_vad/biquad.h"
#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/pitch_search.h"
#include "common_audio/rnn_vad/ring_buffer.h"
#include "common_audio/rnn_vad/rnn_vad_fft.h"
#include "common_audio/rnn_vad/sequence_buffer.h"

namespace webrtc {
namespace rnn_vad {

constexpr size_t kFeatureVectorSize = 42;

constexpr size_t kBandEnergyCoeffsHistorySize = 8;
using RingBufferType =
    RingBuffer<float, kNumOpusBands, kBandEnergyCoeffsHistorySize>;

// Computes FFT boundary indexes corresponding to Opus bands.
std::array<size_t, kNumOpusBands> ComputeOpusBandBoundaries(
    const size_t sample_rate,
    const size_t frame_size);

// Given an array of FFT coefficients and a vector of band boundary indexes,
// compute band energy coefficients.
void ComputeBandEnergies(rtc::ArrayView<const std::complex<float>> fft_coeffs,
                         rtc::ArrayView<const size_t> band_boundaries,
                         rtc::ArrayView<float> band_energies);

// Given the FFT and the band energy coefficients for a reference frame and a
// lagged frame, compute the band correlation coefficients (written into
// |band_correlations|).
void ComputeBandCorrelations(
    rtc::ArrayView<const std::complex<float>> fft_ref,
    rtc::ArrayView<const float> band_energy_coeffs_ref,
    rtc::ArrayView<const std::complex<float>> fft_lagged,
    rtc::ArrayView<const float> band_energy_coeffs_lagged,
    rtc::ArrayView<const size_t> band_boundaries,
    rtc::ArrayView<float> band_correlations);

// Compute DCT. In-place computation is not allowed, |out| is allowed to be
// shorter than |in|; if so, the first N DCT coefficients will only be computed
// where N equals the size of |out|. The function needs a precomputed DCT table
// and a scaling factor, applied to the output coefficients, defined as sqrt of
// 2 divided by the size of |in|.
void ComputeDct(rtc::ArrayView<const float> in,
                rtc::ArrayView<const float> dct_table,
                const float dct_scaling_factor_,
                rtc::ArrayView<float> out);

// Compute a score that indicates the spectral non-stationarity.
float ComputeSpectralVariability(const RingBufferType* ring_buffer);

// Feature extractor to feed the VAD RNN.
class RnnVadFeaturesExtractor {
 public:
  RnnVadFeaturesExtractor();
  ~RnnVadFeaturesExtractor();
  // Disable copy (and move) semantics.
  RnnVadFeaturesExtractor(const RnnVadFeaturesExtractor&) = delete;
  RnnVadFeaturesExtractor& operator=(const RnnVadFeaturesExtractor&) = delete;
  bool is_silence() const { return is_silence_; }
  rtc::ArrayView<const float, kFeatureVectorSize> GetOutput() const;
  void Reset();
  // Analyzes |samples| and computes the corresponding feature vector.
  // |samples| must have a sample rate equal to |kSampleRate|; the caller is
  // responsible for resampling (if needed).
  void ComputeFeatures(rtc::ArrayView<const float, k10msFrameSize> samples);

 private:
  // Updates the pitch period and gain estimations.
  void UpdatePitchEstimation();
  // Computes the band energy coefficients on a frame extracted from the
  // analysis buffer given an inverted lag.
  void ComputeFftAndBandEnergies(const size_t inverted_lag,
                                 rtc::ArrayView<std::complex<float>> fft_coeffs,
                                 rtc::ArrayView<float> band_energy_coeffs);
  // Pre-processing.
  BiQuadFilter hpf_;
  // Analysis buffers.
  SequenceBuffer<float, kBufSize, k10msFrameSize> full_band_buf_;
  SequenceBuffer<float, kHalfBufSize, kHalf10msFrameSize> half_band_buf_;
  // FFT computation (performed in half-band).
  RnnVadFft fft_;
  // Band energy coefficients.
  const std::array<size_t, kNumOpusBands> bands_indexes_;
  // Pitch estimation.
  PitchInfo pitch_info_;
  // Ring buffer for band energy coefficients.
  RingBufferType log_band_energies_ring_buf_;
  // FFT output buffers for the lagged frames.
  std::array<std::complex<float>, kHalf20msNumFftPoints> frame_fft_ref_;
  std::array<std::complex<float>, kHalf20msNumFftPoints> frame_fft_lagged_;
  // DCT table.
  const std::array<float, kNumOpusBands * kNumOpusBands> dct_table_;
  const float dct_scaling_factor_;
  // Feature extractor output.
  bool is_silence_;
  std::array<float, kFeatureVectorSize> feature_vector_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_FEATURES_EXTRACTION_H_
