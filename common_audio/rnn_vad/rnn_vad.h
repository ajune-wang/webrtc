/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_RNN_VAD_RNN_VAD_H_
#define COMMON_AUDIO_RNN_VAD_RNN_VAD_H_

#include <array>
#include <functional>

#include "api/array_view.h"
#include "common_audio/rnn_vad/features_extraction.h"

namespace webrtc {
namespace rnn_vad {

// Maximum number of units for a fully-connected layer. This value is used to
// over-allocate space for fully-connected layers output vectors (implemented as
// std::array). The value should equal the number of units of the largest
// fully-connected layer.
constexpr size_t kFullyConnectedLayersMaxUnits = 24;

// Maximum number of units for a recurrent layer. This value is used to
// over-allocate space for recurrent layers state vectors (implemented as
// std::array). The value should equal the number of units of the largest
// recurrent layer.
constexpr size_t kRecurrentLayersMaxUnits = 24;

// Activation functions.
// Don't move them to the anon namespace in the .cc file since these functions
// must be visible for unit testing.
float TansigApproximated(float x);
float SigmoidApproximated(float x);
float RectifiedLinearUnit(float x);

// Fully-connected layer.
class FullyConnectedLayer {
 public:
  FullyConnectedLayer(const size_t input_size,
                      const size_t output_size,
                      const rtc::ArrayView<const int8_t> bias,
                      const rtc::ArrayView<const int8_t> weights,
                      std::function<float(float)> activation_function);
  FullyConnectedLayer(const FullyConnectedLayer&) = delete;
  FullyConnectedLayer& operator=(const FullyConnectedLayer&) = delete;
  ~FullyConnectedLayer();
  size_t input_size() const { return input_size_; }
  size_t output_size() const { return output_size_; }
  rtc::ArrayView<const float> GetOutput() const;
  // Computes the fully-connected layer output.
  void ComputeOutput(rtc::ArrayView<const float> input);

 private:
  const size_t input_size_;
  const size_t output_size_;
  const rtc::ArrayView<const int8_t> bias_;
  const rtc::ArrayView<const int8_t> weights_;
  std::function<float(float)> activation_function_;
  // The output vector of a recurrent layer has length equal to |output_size_|.
  // However, for efficiency, over-allocation is used.
  std::array<float, kFullyConnectedLayersMaxUnits> output_;
};

// Recurrent layer with gated recurrent units (GRUs).
class GatedRecurrentLayer {
 public:
  GatedRecurrentLayer(const size_t input_size,
                      const size_t output_size,
                      const rtc::ArrayView<const int8_t> bias,
                      const rtc::ArrayView<const int8_t> weights,
                      const rtc::ArrayView<const int8_t> recurrent_weights,
                      std::function<float(float)> activation_function);
  GatedRecurrentLayer(const GatedRecurrentLayer&) = delete;
  GatedRecurrentLayer& operator=(const GatedRecurrentLayer&) = delete;
  ~GatedRecurrentLayer();
  size_t input_size() const { return input_size_; }
  size_t output_size() const { return output_size_; }
  rtc::ArrayView<const float> GetOutput() const;
  void Reset();
  // Computes the recurrent layer output and updates the status.
  void ComputeOutput(rtc::ArrayView<const float> input);

 private:
  const size_t input_size_;
  const size_t output_size_;
  const rtc::ArrayView<const int8_t> bias_;
  const rtc::ArrayView<const int8_t> weights_;
  const rtc::ArrayView<const int8_t> recurrent_weights_;
  std::function<float(float)> activation_function_;
  // The state vector of a recurrent layer has length equal to |output_size_|.
  // However, for efficiency, over-allocation is used.
  std::array<float, kRecurrentLayersMaxUnits> state_;
};

// Recurrent network based VAD.
class RnnBasedVad {
 public:
  RnnBasedVad();
  RnnBasedVad(const RnnBasedVad&) = delete;
  RnnBasedVad& operator=(const RnnBasedVad&) = delete;
  ~RnnBasedVad();
  float vad_probability() const { return vad_probability_; }
  void Reset();
  // Compute and returns the probability of voice (range: [0.0, 1.0]).
  void ComputeVadProbability(
      rtc::ArrayView<const float, kFeatureVectorSize> feature_vector);

 private:
  FullyConnectedLayer input_layer_;
  GatedRecurrentLayer hidden_layer_;
  FullyConnectedLayer output_layer_;
  float vad_probability_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // COMMON_AUDIO_RNN_VAD_RNN_VAD_H_
