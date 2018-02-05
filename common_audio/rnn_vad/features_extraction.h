/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_AUDIO_RNN_VAD_FEATURES_EXTRACTION_H_
#define COMMON_AUDIO_AUDIO_RNN_VAD_FEATURES_EXTRACTION_H_

#include <array>

#include "api/array_view.h"

namespace webrtc {
namespace rnn_vad {

constexpr size_t kAudioFrameSize = 480;
constexpr size_t kFeatureVectorSize = 42;

}  // namespace rnn_vad

using namespace webrtc::rnn_vad;

class RnnVadFeaturesExtractor {
 public:
  RnnVadFeaturesExtractor();
  ~RnnVadFeaturesExtractor();
  bool is_silence() const { return is_silence_; }
  void Reset();
  rtc::ArrayView<const float, kFeatureVectorSize> GetOutput() const;
  // Analyzes |samples| and computes the corresponding feature vector.
  void ComputeFeatures(rtc::ArrayView<const int16_t, kAudioFrameSize> samples);

 private:
  bool is_silence_;
  std::array<float, kFeatureVectorSize> feature_vector_;
};

}  // namespace webrtc

#endif  // COMMON_AUDIO_AUDIO_RNN_VAD_FEATURES_EXTRACTION_H_
