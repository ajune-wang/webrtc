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

// Compute FFT boundary indexes corresponding to Opus bands.
std::array<size_t, kNumOpusBands> ComputeOpusBandBoundaries(
    const size_t sample_rate,
    const size_t frame_size);

// Given an array of FFT coefficients and a vector of band boundary indexes,
// compute band energy coefficients.
void ComputeBandEnergies(rtc::ArrayView<const std::complex<float>> fft_coeffs,
                         rtc::ArrayView<const size_t> band_boundaries,
                         rtc::ArrayView<float> band_energies);

// Given the FFT and the band energy coefficients for a reference frame and a
// lagged frame, compute the band correlation coefficients.
void ComputeBandCorrelations(
    rtc::ArrayView<const std::complex<float>> fft_ref,
    rtc::ArrayView<const float> band_energy_coeffs_ref,
    rtc::ArrayView<const std::complex<float>> fft_lagged,
    rtc::ArrayView<const float> band_energy_coeffs_lagged,
    rtc::ArrayView<const size_t> band_boundaries,
    rtc::ArrayView<float> band_correlations);

// Compute DCT for |in| given a pre-computed DCT table and scaling factor.
// In-place computation is not allowed and |out| can be smaller than |in| in
// order to only compute the first DCT coefficients.
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
  RnnVadFeaturesExtractor(const RnnVadFeaturesExtractor&) = delete;
  RnnVadFeaturesExtractor& operator=(const RnnVadFeaturesExtractor&) = delete;
  ~RnnVadFeaturesExtractor();
  bool is_silence() const { return is_silence_; }
  rtc::ArrayView<const float, kFeatureVectorSize> GetOutput() const;
  void Reset();
  void ComputeFeatures(
      rtc::ArrayView<const float, kFrameSize10ms24kHz> samples);

 private:
  // Update the pitch period and gain estimations.
  void UpdatePitchEstimation();
  // Compute the band energy coefficients on a frame extracted from the
  // analysis buffer given an inverted lag.
  void ComputeFftAndBandEnergies(const size_t inverted_lag,
                                 rtc::ArrayView<std::complex<float>> fft_coeffs,
                                 rtc::ArrayView<float> band_energy_coeffs);
  BiQuadFilter hpf_;
  SequenceBuffer<float, kBufSize24kHz, kFrameSize10ms24kHz> seq_buf_24kHz_;
  SequenceBuffer<float, kBufSize12kHz, kFrameSize10ms12kHz> seq_buf_12kHz_;
  RnnVadFft fft_;
  PitchInfo pitch_info_48kHz_;
  RingBufferType log_band_energies_ring_buf_;
  std::array<std::complex<float>, kFftNumCoeffs20ms12kHz> frame_fft_ref_;
  std::array<std::complex<float>, kFftNumCoeffs20ms12kHz> frame_fft_lagged_;
  const std::array<size_t, kNumOpusBands> bands_boundaries_;
  const std::array<float, kNumOpusBands * kNumOpusBands> dct_table_;
  const float dct_scaling_factor_;
  // Feature extractor output.
  bool is_silence_;
  std::array<float, kFeatureVectorSize> feature_vector_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_FEATURES_EXTRACTION_H_
