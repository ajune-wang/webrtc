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

namespace webrtc {
namespace rnn_vad {

// TODONT(alessiob): Do not change the sample rate until we retrain RNNoise.
constexpr size_t kSampleRate = 24000;

// The original RNNoise implementation uses 10 ms frames; however, for a more
// efficient FFT computation, a power of 2 is used.
constexpr size_t kAudioFrameSize = 256;  // 11 ms.

// At each step, two adjacent frames are analyzed. 50% overlap is applied.
constexpr size_t kAudioFrameBufferSize = 2 * kAudioFrameSize;  // 22 ms.

// Pitch search range.
constexpr size_t kPitchMinPeriod = 30;   // 0.00125s (max frequency: 800 Hz).
constexpr size_t kPitchMaxPeriod = 384;  // 0.016s (min frequency: 62.5 Hz).

// Pitch buffer size.
constexpr size_t kPitchBufferSize = kPitchMaxPeriod + kAudioFrameBufferSize;

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
  void ComputeFeatures(rtc::ArrayView<const float, kAudioFrameSize> samples);

 private:
  // Computes the FFT and Opus-bands energies.
  void AnalyzeFrame();
  // Analysis buffer.
  SequenceBuffer<float, kPitchBufferSize, kAudioFrameSize> analysis_buf_;
  // FFT computation.
  const std::array<float, kAudioFrameSize> analysis_win_;
  std::unique_ptr<RealFourier> fft_;
  AlignedArray<float> fft_input_buf_;
  AlignedArray<std::complex<float>> fft_output_buf_;
  // Output.
  bool is_silence_;
  std::array<float, kFeatureVectorSize> feature_vector_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_FEATURES_EXTRACTION_H_
