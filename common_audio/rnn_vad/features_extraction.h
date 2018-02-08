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
#include "common_audio/rnn_vad/sequence_buffer.h"
#include "system_wrappers/include/aligned_array.h"

namespace webrtc {
namespace rnn_vad {

constexpr size_t kAudioFrameSize = 480;  // 10 ms.
constexpr size_t kFeatureVectorSize = 42;

constexpr size_t kAudioFrameBufferSize = 2 * kAudioFrameSize;

constexpr size_t kPitchMinPeriod = 60;   // 0.00125s (max frequency: 800 Hz).
constexpr size_t kPitchMaxPeriod = 768;  // 0.016s (min frequency: 62.5 Hz).
constexpr size_t kPitchFrameSize = 960;
constexpr size_t kPitchBufferSize = kPitchMaxPeriod + kPitchFrameSize;

}  // namespace rnn_vad

using rnn_vad::kAudioFrameSize;
using rnn_vad::kFeatureVectorSize;
using rnn_vad::kAudioFrameBufferSize;
using rnn_vad::SequenceBuffer;

class RealFourier;

class RnnVadFeaturesExtractor {
 public:
  RnnVadFeaturesExtractor();
  ~RnnVadFeaturesExtractor();
  bool is_silence() const { return is_silence_; }
  void Reset();
  rtc::ArrayView<const float, kFeatureVectorSize> GetOutput() const;
  // Analyzes |samples| and computes the corresponding feature vector.
  void ComputeFeatures(rtc::ArrayView<const float, kAudioFrameSize> samples);

 private:
  // Computes the FFT and Opus-bands energies.
  void AnalyzeFrame();
  bool is_silence_;
  // Input buffer.
  SequenceBuffer<float, kAudioFrameBufferSize, kAudioFrameSize>
      audio_frame_seq_buf_;
  // FFT computation.
  const std::array<float, kAudioFrameSize> analysis_win_;
  std::unique_ptr<RealFourier> fft_;
  AlignedArray<float> fft_input_buf_;
  AlignedArray<std::complex<float>> fft_output_buf_;
  // SequenceBuffer<float, kPitchBufferSize, kAudioFrameSize> pitch_seq_buf_;
  std::array<float, kFeatureVectorSize> feature_vector_;
};

}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_FEATURES_EXTRACTION_H_
