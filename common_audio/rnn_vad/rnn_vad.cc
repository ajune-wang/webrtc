/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/rnn_vad.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "common_audio/rnn_vad/rnn_vad_weights.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// Activation functions.

// TODO(alessiob): Adapted from RNNoise. Add python code to generate this table.
const std::array<float, 201> kTansigTable{
    0.000000f, 0.039979f, 0.079830f, 0.119427f, 0.158649f, 0.197375f, 0.235496f,
    0.272905f, 0.309507f, 0.345214f, 0.379949f, 0.413644f, 0.446244f, 0.477700f,
    0.507977f, 0.537050f, 0.564900f, 0.591519f, 0.616909f, 0.641077f, 0.664037f,
    0.685809f, 0.706419f, 0.725897f, 0.744277f, 0.761594f, 0.777888f, 0.793199f,
    0.807569f, 0.821040f, 0.833655f, 0.845456f, 0.856485f, 0.866784f, 0.876393f,
    0.885352f, 0.893698f, 0.901468f, 0.908698f, 0.915420f, 0.921669f, 0.927473f,
    0.932862f, 0.937863f, 0.942503f, 0.946806f, 0.950795f, 0.954492f, 0.957917f,
    0.961090f, 0.964028f, 0.966747f, 0.969265f, 0.971594f, 0.973749f, 0.975743f,
    0.977587f, 0.979293f, 0.980869f, 0.982327f, 0.983675f, 0.984921f, 0.986072f,
    0.987136f, 0.988119f, 0.989027f, 0.989867f, 0.990642f, 0.991359f, 0.992020f,
    0.992631f, 0.993196f, 0.993718f, 0.994199f, 0.994644f, 0.995055f, 0.995434f,
    0.995784f, 0.996108f, 0.996407f, 0.996682f, 0.996937f, 0.997172f, 0.997389f,
    0.997590f, 0.997775f, 0.997946f, 0.998104f, 0.998249f, 0.998384f, 0.998508f,
    0.998623f, 0.998728f, 0.998826f, 0.998916f, 0.999000f, 0.999076f, 0.999147f,
    0.999213f, 0.999273f, 0.999329f, 0.999381f, 0.999428f, 0.999472f, 0.999513f,
    0.999550f, 0.999585f, 0.999617f, 0.999646f, 0.999673f, 0.999699f, 0.999722f,
    0.999743f, 0.999763f, 0.999781f, 0.999798f, 0.999813f, 0.999828f, 0.999841f,
    0.999853f, 0.999865f, 0.999875f, 0.999885f, 0.999893f, 0.999902f, 0.999909f,
    0.999916f, 0.999923f, 0.999929f, 0.999934f, 0.999939f, 0.999944f, 0.999948f,
    0.999952f, 0.999956f, 0.999959f, 0.999962f, 0.999965f, 0.999968f, 0.999970f,
    0.999973f, 0.999975f, 0.999977f, 0.999978f, 0.999980f, 0.999982f, 0.999983f,
    0.999984f, 0.999986f, 0.999987f, 0.999988f, 0.999989f, 0.999990f, 0.999990f,
    0.999991f, 0.999992f, 0.999992f, 0.999993f, 0.999994f, 0.999994f, 0.999994f,
    0.999995f, 0.999995f, 0.999996f, 0.999996f, 0.999996f, 0.999997f, 0.999997f,
    0.999997f, 0.999997f, 0.999997f, 0.999998f, 0.999998f, 0.999998f, 0.999998f,
    0.999998f, 0.999998f, 0.999999f, 0.999999f, 0.999999f, 0.999999f, 0.999999f,
    0.999999f, 0.999999f, 0.999999f, 0.999999f, 0.999999f, 0.999999f, 0.999999f,
    0.999999f, 1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f,
    1.000000f, 1.000000f, 1.000000f, 1.000000f, 1.000000f,
};

}  // namespace

// TODO(alessiob): Adapted from RNNoise.
float TansigApproximated(float x) {
  // Tests are reversed to catch NaNs.
  if (!(x < 8.f))
    return 1.f;
  if (!(x > -8.f))
    return -1.f;
  float sign = 1.f;
  if (x < 0.f) {
    x = -x;
    sign = -1.f;
  }
  // Look-up.
  int i = static_cast<int>(std::floor(0.5f + 25 * x));
  float y = kTansigTable[i];
  // Map i back to x's scale (undo 25 factor).
  x -= 0.04f * i;
  //
  y = y + x * (1.f - y * y) * (1.f - y * x);
  return sign * y;
}

// TODO(alessiob): Adapted from RNNoise.
float SigmoidApproximated(float x) {
  return 0.5f + 0.5f * TansigApproximated(0.5f * x);
}

float RectifiedLinearUnit(float x) {
  return x < 0.f ? 0.f : x;
}

FullyConnectedLayer::FullyConnectedLayer(
    const size_t input_size,
    const size_t output_size,
    const rtc::ArrayView<const int8_t> bias,
    const rtc::ArrayView<const int8_t> weights,
    std::function<float(float)> activation_function)
    : input_size_(input_size),
      output_size_(output_size),
      bias_(bias),
      weights_(weights),
      activation_function_(activation_function) {
  RTC_CHECK_LE(output_size_, kFullyConnectedLayersMaxUnits)
      << "Static over-allocation of fully-connected layers output vectors is "
         "not sufficient.";
  RTC_CHECK_EQ(output_size_, bias_.size())
      << "Mismatching output size and bias terms array size.";
  RTC_CHECK_EQ(input_size_ * output_size_, weights_.size())
      << "Mismatching input-output size and weight coefficients array size.";
}

FullyConnectedLayer::~FullyConnectedLayer() = default;

rtc::ArrayView<const float> FullyConnectedLayer::GetOutput() const {
  return rtc::ArrayView<const float>(output_.data(), output_size_);
}

void FullyConnectedLayer::ComputeOutput(rtc::ArrayView<const float> input) {
  // TODO(alessiob): Optimize using SSE/AVX fused multiply-add operations.
  for (size_t o = 0; o < output_size_; ++o) {
    output_[o] = bias_[o];
    for (size_t i = 0; i < input_size_; ++i)
      output_[o] += input[i] * weights_[i * output_size_ + o];
    output_[o] = activation_function_(kWeightsScale * output_[o]);
  }
}

GatedRecurrentLayer::GatedRecurrentLayer(
    const size_t input_size,
    const size_t output_size,
    const rtc::ArrayView<const int8_t> bias,
    const rtc::ArrayView<const int8_t> weights,
    const rtc::ArrayView<const int8_t> recurrent_weights,
    std::function<float(float)> activation_function)
    : input_size_(input_size),
      output_size_(output_size),
      bias_(bias),
      weights_(weights),
      recurrent_weights_(recurrent_weights),
      activation_function_(activation_function) {
  RTC_CHECK_LE(output_size_, kRecurrentLayersMaxUnits)
      << "Static over-allocation of recurrent layers state vectors is not "
      << "sufficient.";
  RTC_CHECK_EQ(3 * output_size_, bias_.size())
      << "Mismatching output size and bias terms array size.";
  RTC_CHECK_EQ(3 * input_size_ * output_size_, weights_.size())
      << "Mismatching input-output size and weight coefficients array size.";
  RTC_CHECK_EQ(3 * input_size_ * output_size_, recurrent_weights_.size())
      << "Mismatching input-output size and recurrent weight coefficients array"
      << " size.";
  Reset();
}

GatedRecurrentLayer::~GatedRecurrentLayer() = default;

rtc::ArrayView<const float> GatedRecurrentLayer::GetOutput() const {
  return rtc::ArrayView<const float>(state_.data(), output_size_);
}

void GatedRecurrentLayer::Reset() {
  state_.fill(0.f);
}

void GatedRecurrentLayer::ComputeOutput(rtc::ArrayView<const float> input) {
  // TODO(alessiob): Optimize using SSE/AVX fused multiply-add operations.
  // Stride and offset used to read parameter arrays.
  const size_t stride = 3 * output_size_;
  size_t offset = 0;

  // Compute update gates.
  std::array<float, kRecurrentLayersMaxUnits> update;
  for (size_t o = 0; o < output_size_; ++o) {
    update[o] = bias_[o];
    for (size_t i = 0; i < input_size_; ++i)  // Add input.
      update[o] += input[i] * weights_[i * stride + o];
    for (size_t s = 0; s < output_size_; ++s)  // Add state.
      update[o] += state_[s] * recurrent_weights_[s * stride + o];
    update[o] = SigmoidApproximated(kWeightsScale * update[o]);
  }

  // Compute reset gates.
  offset += output_size_;
  std::array<float, kRecurrentLayersMaxUnits> reset;
  for (size_t o = 0; o < output_size_; ++o) {
    reset[o] = bias_[offset + o];
    for (size_t i = 0; i < input_size_; ++i)  // Add input.
      reset[o] += input[i] * weights_[offset + i * stride + o];
    for (size_t s = 0; s < output_size_; ++s)  // Add state.
      reset[o] += state_[s] * recurrent_weights_[offset + s * stride + o];
    reset[o] = SigmoidApproximated(kWeightsScale * reset[o]);
  }

  // Compute output.
  offset += output_size_;
  std::array<float, kRecurrentLayersMaxUnits> output;
  for (size_t o = 0; o < output_size_; ++o) {
    output[o] = bias_[offset + o];
    for (size_t i = 0; i < input_size_; ++i)  // Add input.
      output[o] += input[i] * weights_[offset + i * stride + o];
    for (size_t s = 0; s < output_size_; ++s)  // Add state through reset gates.
      output[o] +=
          state_[s] * recurrent_weights_[offset + s * stride + o] * reset[s];
    output[o] = activation_function_(kWeightsScale * output[o]);
    // Update output through the update gates.
    output[o] = update[o] * state_[o] + (1.f - update[o]) * output[o];
  }

  // Update the state. Not done in the previous loop since that would pollute
  // the current state and lead to incorrect output values.
  std::copy(output.begin(), output.end(), state_.begin());
}

RnnBasedVad::RnnBasedVad()
    : input_layer_(kInputLayerInputSize,
                   kInputLayerOutputSize,
                   kInputLayerBias,
                   kInputLayerWeights,
                   TansigApproximated),
      hidden_layer_(kHiddenLayerInputSize,
                    kHiddenLayerOutputSize,
                    kHiddenLayerBias,
                    kHiddenLayerWeights,
                    kHiddenLayerRecurrentWeights,
                    RectifiedLinearUnit),
      output_layer_(kOutputLayerInputSize,
                    kOutputLayerOutputSize,
                    kOutputLayerBias,
                    kOutputLayerWeights,
                    SigmoidApproximated) {
  // Input-output chaining size checks.
  RTC_CHECK_EQ(input_layer_.output_size(), hidden_layer_.input_size())
      << "The input and the hidden layers sizes do not match.";
  RTC_CHECK_EQ(hidden_layer_.output_size(), output_layer_.input_size())
      << "The hidden and the output layers sizes do not match.";
}

RnnBasedVad::~RnnBasedVad() = default;

void RnnBasedVad::Reset() {
  hidden_layer_.Reset();
}

void RnnBasedVad::ComputeVadProbability(
    rtc::ArrayView<const float, kFeatureVectorSize> feature_vector) {
  input_layer_.ComputeOutput(feature_vector);
  hidden_layer_.ComputeOutput(input_layer_.GetOutput());
  output_layer_.ComputeOutput(hidden_layer_.GetOutput());
  const auto vad_output = output_layer_.GetOutput();
  vad_probability_ = vad_output[0];
}

}  // namespace rnn_vad
}  // namespace webrtc
