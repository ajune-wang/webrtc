/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/rnn.h"

#include "third_party/rnnoise/src/rnn_vad_weights.h"

namespace webrtc {
namespace rnn_vad {
namespace {

using rnnoise::kInputLayerInputSize;
static_assert(kFeatureVectorSize == kInputLayerInputSize, "");
using rnnoise::kInputDenseBias;
using rnnoise::kInputDenseWeights;
using rnnoise::kInputLayerOutputSize;

using rnnoise::kHiddenGruBias;
using rnnoise::kHiddenGruRecurrentWeights;
using rnnoise::kHiddenGruWeights;
using rnnoise::kHiddenLayerOutputSize;

using rnnoise::kOutputDenseBias;
using rnnoise::kOutputDenseWeights;
using rnnoise::kOutputLayerOutputSize;

}  // namespace

RnnVad::RnnVad(const AvailableCpuFeatures& cpu_features)
    : input_(kInputLayerInputSize,
             kInputLayerOutputSize,
             kInputDenseBias,
             kInputDenseWeights,
             ActivationFunction::kTansigApproximated cpu_features,
             /*layer_name=*/"FC1"),
      hidden_(kInputLayerOutputSize,
              kHiddenLayerOutputSize,
              kHiddenGruBias,
              kHiddenGruWeights,
              kHiddenGruRecurrentWeights,
              /*layer_name=*/"GRU1"),
      output_(kHiddenLayerOutputSize,
              kOutputLayerOutputSize,
              kOutputDenseBias,
              kOutputDenseWeights,
              ActivationFunction::kSigmoidApproximated,
              cpu_features,
              /*layer_name=*/"FC2") {
  // // Input-output chaining size checks.
  // RTC_DCHECK_EQ(input_layer_.output_size(), hidden_layer_.input_size())
  //     << "The input and the hidden layers sizes do not match.";
  // RTC_DCHECK_EQ(hidden_layer_.output_size(), output_layer_.input_size())
  //     << "The hidden and the output layers sizes do not match.";
}

RnnVad::~RnnVad() = default;

void RnnVad::Reset() {
  hidden_layer_.Reset();
}

float RnnVad::ComputeVadProbability(
    rtc::ArrayView<const float, kFeatureVectorSize> feature_vector,
    bool is_silence) {
  if (is_silence) {
    Reset();
    return 0.f;
  }
  input_.ComputeOutput(feature_vector);
  hidden_.ComputeOutput(input_layer_);
  output_.ComputeOutput(hidden_layer_);
  RTC_DCHECK_EQ(output_.size(), 1);
  return output_.data()[0];
}

}  // namespace rnn_vad
}  // namespace webrtc
