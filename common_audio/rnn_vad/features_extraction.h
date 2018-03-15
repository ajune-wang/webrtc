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
#include "common_audio/rnn_vad/rnn_vad_fft.h"
#include "common_audio/rnn_vad/sequence_buffer.h"

namespace webrtc {
namespace rnn_vad {

constexpr size_t kFeatureVectorSize = 42;

// Computes FFT indexes corresponding to Opus bands.
std::array<size_t, kNumOpusBands> ComputeOpusBandsIndexes(
    const size_t sample_rate,
    const size_t frame_size);

// Given an array of FFT coefficients and a vector of band indexes, compute
// band energy coefficients. The FFT coefficients are scaled by |scaling|.
void ComputeBandEnergies(rtc::ArrayView<const std::complex<float>> fft_coeffs,
                         const float scaling,
                         rtc::ArrayView<const size_t> bands_indexes,
                         rtc::ArrayView<float> band_energies);

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
  // Estimates pitch period and gain.
  void AnalyzePitch();
  // Pre-processing.
  BiQuadFilter hpf_;
  // Analysis buffers.
  SequenceBuffer<float, kBufSize, k10msFrameSize> full_band_buf_;
  SequenceBuffer<float, kHalfBufSize, kHalf10msFrameSize> half_band_buf_;
  // FFT computation (performed in half-band).
  RnnVadFft fft_;
  // Band energy coefficients.
  const std::array<size_t, kNumOpusBands> bands_indexes_;
  std::array<float, kNumOpusBands> band_energies_;
  // Pitch estimation.
  PitchInfo pitch_info_;
  // Feature extractor output.
  bool is_silence_;
  std::array<float, kFeatureVectorSize> feature_vector_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_FEATURES_EXTRACTION_H_
