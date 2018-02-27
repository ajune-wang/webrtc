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
#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/sequence_buffer.h"
#include "system_wrappers/include/aligned_array.h"

namespace webrtc {
namespace rnn_vad {

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
