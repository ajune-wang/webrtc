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
#include <memory>

#include "api/array_view.h"
#include "common_audio/rnn_vad/biquad.h"
#include "common_audio/rnn_vad/common.h"
#include "common_audio/rnn_vad/pitch_search.h"
#include "common_audio/rnn_vad/sequence_buffer.h"
#include "common_audio/rnn_vad/spectral_features.h"

namespace webrtc {
namespace rnn_vad {

constexpr size_t kFeatureVectorSize = 42;

// Feature extractor to feed the VAD RNN.
class RnnVadFeaturesExtractor {
 public:
  RnnVadFeaturesExtractor();
  RnnVadFeaturesExtractor(const RnnVadFeaturesExtractor&) = delete;
  RnnVadFeaturesExtractor& operator=(const RnnVadFeaturesExtractor&) = delete;
  ~RnnVadFeaturesExtractor();
  rtc::ArrayView<const float, kFeatureVectorSize> GetFeatureVectorView() const;
  void Reset();
  // Analyze samples and, if silence is not detected, compute the features and
  // return false. If silence is detected return true.
  bool ComputeFeaturesCheckSilence(
      rtc::ArrayView<const float, kFrameSize10ms24kHz> samples);

 private:
  BiQuadFilter hpf_;
  SequenceBuffer<float, kBufSize24kHz, kFrameSize10ms24kHz> seq_buf_24kHz_;
  PitchInfo pitch_info_48kHz_;
  SpectralFeaturesExtractor spectral_features_extractor_;
  std::array<float, kFeatureVectorSize> feature_vector_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_FEATURES_EXTRACTION_H_
