/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <vector>

#include "common_audio/resampler/push_sinc_resampler.h"
#include "common_audio/rnn_vad/features_extraction.h"
#include "common_audio/rnn_vad/rnn_vad.h"
#include "common_audio/rnn_vad/test_utils.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

using rnn_vad::FullyConnectedLayer;
using rnn_vad::GatedRecurrentLayer;
using rnn_vad::kFrameSize10ms24kHz;
using rnn_vad::kFeatureVectorSize;
using rnn_vad::RectifiedLinearUnit;
using rnn_vad::RnnBasedVad;
using rnn_vad::RnnVadFeaturesExtractor;
using rnn_vad::SigmoidApproximated;

namespace {

void TestFullyConnectedLayer(FullyConnectedLayer* fc,
                             rtc::ArrayView<const float> input_vector,
                             const float expected_output) {
  RTC_CHECK(fc);
  fc->ComputeOutput(input_vector);
  const auto output = fc->GetOutput();
  EXPECT_NEAR(expected_output, output[0], 3e-6f);
}

void TestGatedRecurrentLayer(
    GatedRecurrentLayer* gru,
    rtc::ArrayView<const float> input_sequence,
    rtc::ArrayView<const float> expected_output_sequence) {
  RTC_CHECK(gru);
  auto gru_output_view = gru->GetOutput();
  const size_t input_sequence_length =
      rtc::CheckedDivExact(input_sequence.size(), gru->input_size());
  const size_t output_sequence_length =
      rtc::CheckedDivExact(expected_output_sequence.size(), gru->output_size());
  ASSERT_EQ(input_sequence_length, output_sequence_length)
      << "The test data length is invalid.";
  // Feed the GRU layer and check the output at every step.
  gru->Reset();
  for (size_t i = 0; i < input_sequence_length; ++i) {
    SCOPED_TRACE(i);
    gru->ComputeOutput(
        input_sequence.subview(i * gru->input_size(), gru->input_size()));
    const auto expected_output = expected_output_sequence.subview(
        i * gru->output_size(), gru->output_size());
    ExpectNearAbsolute(expected_output, gru_output_view, 3e-6f);
  }
}

}  // namespace

// Bit-exactness check for fully connected layers.
TEST(RnnVadTest, CheckFullyConnectedLayerOutput) {
  const std::array<int8_t, 1> bias = {-50};
  const std::array<int8_t, 24> weights = {
      127,  127,  127, 127,  127,  20,  127,  -126, -126, -54, 14,  125,
      -126, -126, 127, -125, -126, 127, -127, -127, -57,  -30, 127, 80};
  FullyConnectedLayer fc(24, 1, bias, weights, SigmoidApproximated);
  // Test on different inputs.
  {
    const std::array<float, 24> input_vector = {
        0,           0,           0,
        0,           0,           0,
        0.215833917, 0.290601075, 0.238759011,
        0.244751841, 0,           0.0461241305,
        0.106401242, 0.223070428, 0.630603909,
        0.690453172, 0,           0.387645692,
        0.166913897, 0,           0.0327451192,
        0,           0.136149868, 0.446351469};
    TestFullyConnectedLayer(&fc, {input_vector}, 0.436567038);
  }
  {
    const std::array<float, 24> input_vector = {
        0.592162728,   0.529089332,  1.18205106,   1.21736848,  0,
        0.470851123,   0.130675942,  0.320903003,  0.305496395, 0.0571633279,
        1.57001138,    0.0182026215, 0.0977443159, 0.347477973, 0.493206412,
        0.9688586,     0.0320267938, 0.244722098,  0.312745273, 0,
        0.00650715502, 0.312553257,  1.62619662,   0.782880902};
    TestFullyConnectedLayer(&fc, {input_vector}, 0.874741316);
  }
  {
    const std::array<float, 24> input_vector = {
        0.395022154,    0.333681047,  0.76302278,  0.965480626, 0,
        0.941198349,    0.0892967582, 0.745046318, 0.635769248, 0.238564298,
        0.970656633,    0.014159563,  0.094203949, 0.446816623, 0.640755892,
        1.20532358,     0.0254284926, 0.283327013, 0.726210058, 0.0550272502,
        0.000344108557, 0.369803518,  1.56680179,  0.997883797};
    TestFullyConnectedLayer(&fc, {input_vector}, 0.672785878);
  }
}

TEST(RnnVadTest, CheckGatedRecurrentLayer) {
  const std::array<int8_t, 12> bias = {96,   -99, -81, -114, 49,  119,
                                       -118, 68,  -76, 91,   121, 125};
  const std::array<int8_t, 60> weights = {
      124, 9,    1,    116, -66, -21, -118, -110, 104,  75,  -23,  -51,
      -72, -111, 47,   93,  77,  -98, 41,   -8,   40,   -23, -43,  -107,
      9,   -73,  30,   -32, -2,  64,  -26,  91,   -48,  -24, -28,  -104,
      74,  -46,  116,  15,  32,  52,  -126, -38,  -121, 12,  -16,  110,
      -95, 66,   -103, -35, -38, 3,   -126, -61,  28,   98,  -117, -43};
  const std::array<int8_t, 60> recurrent_weights = {
      -3,  87,  50,  51,  -22,  27,  -39, 62,   31,  -83, -52,  -48,
      -6,  83,  -19, 104, 105,  48,  23,  68,   23,  40,  7,    -120,
      64,  -62, 117, 85,  -51,  -43, 54,  -105, 120, 56,  -128, -107,
      39,  50,  -17, -47, -117, 14,  108, 12,   -7,  -72, 103,  -87,
      -66, 82,  84,  100, -98,  102, -49, 44,   122, 106, -20,  -69};
  GatedRecurrentLayer gru(5, 4, bias, weights, recurrent_weights,
                          RectifiedLinearUnit);
  // Test on different inputs.
  {
    const std::array<float, 20> input_sequence = {
        0.89395463, 0.93224651, 0.55788344, 0.32341808, 0.93355054,
        0.13475326, 0.97370994, 0.14253306, 0.93710381, 0.76093364,
        0.65780413, 0.41657975, 0.49403164, 0.46843281, 0.75138855,
        0.24517593, 0.47657707, 0.57064998, 0.435184,   0.19319285};
    const std::array<float, 16> expected_output_sequence = {
        0.02391230, 0.57730770, 0.00000000, 0.00000000, 0.01282811, 0.64330572,
        0.00000000, 0.04863098, 0.00781069, 0.75267816, 0.00000000, 0.02579715,
        0.00471378, 0.59162533, 0.11087593, 0.01334511};
    TestGatedRecurrentLayer(&gru, input_sequence, expected_output_sequence);
  }
}

// Run the VAD on PCM samples and checks that the output probabilities are in a
// valid range.
TEST(RnnVadTest, CheckValidVadProbabilities) {
  // PCM samples reader and buffers.
  auto samples_reader = CreatePcmSamplesReader(kFrameSize10ms48kHz);
  const size_t num_frames = samples_reader.second;
  std::array<float, kFrameSize10ms48kHz> samples;
  // Pre-fetch and decimate samples.
  PushSincResampler decimator(kFrameSize10ms48kHz, kFrameSize10ms24kHz);
  std::vector<float> prefetched_decimated_samples;
  prefetched_decimated_samples.resize(num_frames * kFrameSize10ms24kHz);
  for (size_t i = 0; i < num_frames; ++i) {
    samples_reader.first->ReadChunk({samples.data(), samples.size()});
    decimator.Resample(samples.data(), samples.size(),
                       &prefetched_decimated_samples[i * kFrameSize10ms24kHz],
                       kFrameSize10ms24kHz);
  }
  // When needed, run multiple tests to measure average running time.
  constexpr size_t number_of_tests = 1;
  for (size_t k = 0; k < number_of_tests; ++k) {
    // Feature extractor and RNN.
    RnnVadFeaturesExtractor features_extractor;
    auto feature_vector_view = features_extractor.GetFeatureVectorView();
    RnnBasedVad vad;
    for (size_t i = 0; i < num_frames; ++i) {
      if (features_extractor.ComputeFeaturesCheckSilence(
              {&prefetched_decimated_samples[i * kFrameSize10ms24kHz],
               kFrameSize10ms24kHz})) {
        vad.Reset();
      } else {
        vad.ComputeVadProbability(feature_vector_view);
        EXPECT_LE(0.f, vad.vad_probability());
        EXPECT_LE(vad.vad_probability(), 1.f);
      }
    }
    samples_reader.first->SeekBeginning();
  }
}

// Bit-exactness test checking that precomputed frame-wise features lead to the
// expected VAD probabilities.
TEST(RnnVadTest, RnnVadBitExactness) {
  // Init.
  auto features_reader = CreateFeatureMatrixReader();
  auto vad_probs_reader = CreateVadProbsReader();
  ASSERT_EQ(features_reader.second, vad_probs_reader.second);
  const size_t num_frames = features_reader.second;
  // Frame-wise buffers.
  float expected_vad_probability;
  float is_silence;
  std::array<float, kFeatureVectorSize> features;

  // Compute VAD probability using the precomputed features.
  RnnBasedVad vad;
  for (size_t i = 0; i < num_frames; ++i) {
    SCOPED_TRACE(i);
    // Read frame data.
    RTC_CHECK(vad_probs_reader.first->ReadValue(&expected_vad_probability));
    // The features file also includes a silence flag for each frame.
    RTC_CHECK(features_reader.first->ReadValue(&is_silence));
    RTC_CHECK(
        features_reader.first->ReadChunk({features.data(), features.size()}));
    // Skip silent frames.
    ASSERT_TRUE(is_silence == 0.f || is_silence == 1.f);
    if (is_silence == 1.f) {
      ASSERT_EQ(expected_vad_probability, 0.f);
      continue;
    }
    // Compute and check VAD probability.
    vad.ComputeVadProbability({features.data(), features.size()});
    EXPECT_NEAR(expected_vad_probability, vad.vad_probability(), 3e-6f);
  }
}

}  // namespace test
}  // namespace webrtc
