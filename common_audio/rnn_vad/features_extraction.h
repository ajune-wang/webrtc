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
#include "common_audio/real_fourier.h"
#include "common_audio/rnn_vad/sequence_buffer.h"
#include "system_wrappers/include/aligned_array.h"

#define TEST_AT_48K 1

namespace webrtc {
namespace rnn_vad {

#ifdef TEST_AT_48K
constexpr size_t kSampleRate = 48000;
constexpr size_t kInputFrameSize = 480;
#else
// Do not change the sample rate until we retrain RNNoise.
constexpr size_t kSampleRate = 24000;
constexpr size_t kInputFrameSize = 256;
#endif
constexpr size_t kFrameSize = 2 * kInputFrameSize;  // Sliding win 50% overlap.

// Pitch analysis params.
#ifdef TEST_AT_48K
constexpr size_t kPitchMinPeriod = 60;   // 0.00125 s (800 Hz).
constexpr size_t kPitchMaxPeriod = 768;  // 0.016 s (62.5 Hz).
#else
constexpr size_t kPitchMinPeriod = 30;   // 0.00125 s (800 Hz).
constexpr size_t kPitchMaxPeriod = 384;  // 0.016 s (62.5 Hz).
#endif
constexpr size_t kBufSize = kPitchMaxPeriod + kFrameSize;
static_assert(kBufSize % 2 == 0, "Invalid full band buffer size.");

// Half-band analysis.
constexpr size_t kHalfSampleRate = kSampleRate / 2;
constexpr size_t kHalfInputFrameSize = kInputFrameSize / 2;
constexpr size_t kHalfFrameSize = kFrameSize / 2;
constexpr size_t kHalfBufSize = kBufSize / 2;

constexpr size_t kNumOpusBands = 22;
constexpr size_t kFeatureVectorSize = 42;

class RnnVadFeaturesExtractor {
 public:
  RnnVadFeaturesExtractor();
  ~RnnVadFeaturesExtractor();
  bool is_silence() const { return is_silence_; }
  rtc::ArrayView<const float, kFeatureVectorSize> GetOutput() const;
  void Reset();
  // Analyzes |samples| and computes the corresponding feature vector.
  // |samples| must have a sample rate equal to |kSampleRate|; the caller is
  // responsible for resampling (if needed).
  void ComputeFeatures(rtc::ArrayView<const float, kInputFrameSize> samples);

 private:
  // Computes the FFT and Opus-bands energies.
  void AnalyzeFrame();
  // Estimates pitch period and gain.
  void AnalyzePitch();
  // Analysis buffers.
  SequenceBuffer<float, kBufSize, kInputFrameSize> full_band_buf_;
  SequenceBuffer<float, kHalfBufSize, kHalfInputFrameSize> half_band_buf_;
  // FFT computation (performed in half-band).
  const std::array<float, kHalfFrameSize / 2> half_analysis_win_;
  std::unique_ptr<RealFourier> fft_;
  AlignedArray<float> fft_input_buf_;
  AlignedArray<std::complex<float>> fft_output_buf_;
  const std::array<float, kNumOpusBands> bands_indexes_;
  std::array<float, kNumOpusBands> band_energies_;
  // Output.
  bool is_silence_;
  std::array<float, kFeatureVectorSize> feature_vector_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_FEATURES_EXTRACTION_H_
