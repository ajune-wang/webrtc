/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RNN_GRU_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RNN_GRU_H_

#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "modules/audio_processing/agc2/cpu_features.h"

namespace webrtc {
namespace rnn_vad {

// Recurrent layer with gated recurrent units (GRUs) with sigmoid and ReLU as
// activation functions for the update/reset and output gates respectively.
class GatedRecurrentLayer {
 public:
  GatedRecurrentLayer(int input_size,
                      int output_size,
                      rtc::ArrayView<const int8_t> bias,
                      rtc::ArrayView<const int8_t> weights,
                      rtc::ArrayView<const int8_t> recurrent_weights,
                      absl::string_view layer_name);
  GatedRecurrentLayer(const GatedRecurrentLayer&) = delete;
  GatedRecurrentLayer& operator=(const GatedRecurrentLayer&) = delete;
  ~GatedRecurrentLayer();

  // Returns the size of the input vector.
  int input_size() const { return input_size_; }
  // Returns the pointer to the first element of the output buffer.
  const float* data() const { return state_.data(); }
  // Returns the size of the output buffer.
  size_t size() const { return state_.size(); }

  // Resets the GRU state.
  void Reset();
  // Computes the recurrent layer output and updates the status.
  void ComputeOutput(rtc::ArrayView<const float> input);

 private:
  const int input_size_;
  const std::vector<float> bias_;
  const std::vector<float> weights_;
  const std::vector<float> recurrent_weights_;
  std::vector<float> state_;
  // Pre-allocated gate buffers.
  std::vector<float> update_;
  std::vector<float> reset_;
  std::vector<float> output_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RNN_GRU_H_
