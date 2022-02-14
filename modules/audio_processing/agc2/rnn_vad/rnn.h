/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RNN_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RNN_H_

#include <stddef.h>
#include <sys/types.h>

#include <array>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/rnn_fc.h"
#include "modules/audio_processing/agc2/rnn_vad/rnn_gru.h"

namespace webrtc {
namespace rnn_vad {

// Recurrent network with hard-coded architecture and weights for voice activity
// detection.
class RnnVad {
 public:
  explicit RnnVad(const AvailableCpuFeatures& cpu_features);
  RnnVad(const RnnVad&) = delete;
  RnnVad& operator=(const RnnVad&) = delete;
  ~RnnVad();
  void Reset();
  // Observes `feature_vector` and `is_silence`, updates the RNN, and stores and
  // returns the current voice probability. Resets the state if `is_silence` is
  // true.
  float ComputeVadProbability(
      rtc::ArrayView<const float, kFeatureVectorSize> feature_vector,
      bool is_silence);
  // Returns the last VAD probability. Can be called multiple times after each
  // `ComputeVadProbability()` call.
  float GetVadProbability() const;

 private:
  FullyConnectedLayer input_;
  GatedRecurrentLayer hidden_;
  FullyConnectedLayer output_;
  float last_vad_probability_ = 0.0f;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RNN_H_
