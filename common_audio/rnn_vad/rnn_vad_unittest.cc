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
#include <fstream>
#include <iostream>
#include <string>

#include "common_audio/rnn_vad/downsampling.h"
#include "common_audio/rnn_vad/features_extraction.h"
#include "common_audio/rnn_vad/rnn_vad.h"
#include "common_audio/rnn_vad/sequence_buffer.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {

using rnn_vad::Decimate2x;
using rnn_vad::FullyConnectedLayer;
using rnn_vad::GatedRecurrentLayer;
using rnn_vad::kInputFrameSize;
using rnn_vad::kFeatureVectorSize;
using rnn_vad::RectifiedLinearUnit;
using rnn_vad::RnnBasedVad;
using rnn_vad::RnnVadFeaturesExtractor;
using rnn_vad::SequenceBuffer;
using rnn_vad::SigmoidApproximated;

namespace {

constexpr double kExpectNearTolerance = 1e-6;

// Reader for binary files consisting of an arbitrary long sequence of elements
// having type T. It is possible to read and cast to another type D at once.
// ReadChunk() reads chunks of size N.
template <typename T, size_t N = 1, typename D = T>
class BinaryFileReader {
 public:
  explicit BinaryFileReader(const std::string& file_path)
      : is_(file_path, std::ios::binary | std::ios::ate),
        data_length_(is_.tellg() / sizeof(T)) {
    RTC_CHECK(is_);
    is_.seekg(0, is_.beg);
  }
  ~BinaryFileReader() = default;
  size_t data_length() const { return data_length_; }
  bool ReadValue(D* dst) {
    if (std::is_same<T, D>::value) {
      is_.read(reinterpret_cast<char*>(dst), sizeof(T));
    } else {
      T v;
      is_.read(reinterpret_cast<char*>(&v), sizeof(T));
      *dst = static_cast<D>(v);
    }
    return is_.gcount() == sizeof(T);
  }
  bool ReadChunk(rtc::ArrayView<D, N> dst) {
    constexpr std::streamsize bytes_to_read = N * sizeof(T);
    if (std::is_same<T, D>::value) {
      is_.read(reinterpret_cast<char*>(dst.data()), bytes_to_read);
    } else {
      std::array<T, N> buf;
      is_.read(reinterpret_cast<char*>(buf.data()), bytes_to_read);
      std::transform(buf.begin(), buf.end(), dst.begin(),
                     [](const T& v) -> D { return static_cast<D>(v); });
    }
    return is_.gcount() == bytes_to_read;
  }

 private:
  std::ifstream is_;
  const size_t data_length_;
};

void TestFullyConnectedLayer(FullyConnectedLayer* fc,
                             rtc::ArrayView<const float> input_vector,
                             const float expected_output) {
  RTC_CHECK(fc);
  fc->ComputeOutput(input_vector);
  const auto output = fc->GetOutput();
  EXPECT_NEAR(expected_output, output[0], kExpectNearTolerance);
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
    gru->ComputeOutput(
        input_sequence.subview(i * gru->input_size(), gru->input_size()));
    const auto expected_output = expected_output_sequence.subview(
        i * gru->output_size(), gru->output_size());
    ASSERT_EQ(expected_output.size(), gru_output_view.size());
    for (size_t j = 0; j < gru_output_view.size(); ++j) {
      std::ostringstream ss;
      ss << "(" << i << ", " << j << ")";
      SCOPED_TRACE(ss.str());
      EXPECT_NEAR(expected_output[j], gru_output_view[j], kExpectNearTolerance);
    }
  }
}

template <typename T, size_t S, size_t N>
void TestSequenceBufferPushOp() {
  static_assert(std::is_integral<T>::value, "Integral required.");
  SequenceBuffer<T, S, N> seq_buf(0);
  auto seq_buf_view = seq_buf.GetBufferView();
  std::array<T, N> chunk;
  rtc::ArrayView<T, N> chunk_view(chunk.data(), N);

  // Check that a chunk is fully gone after ceil(S / N) push ops.
  chunk.fill(1);
  seq_buf.Push(chunk_view);
  chunk.fill(0);
  constexpr size_t required_push_ops = (S % N) ? S / N + 1 : S / N;
  for (size_t i = 0; i < required_push_ops - 1; ++i) {
    seq_buf.Push(chunk_view);
    // Still in the buffer.
    const auto m = std::max_element(seq_buf_view.begin(), seq_buf_view.end());
    EXPECT_EQ(1, *m);
  }
  // Gone after another push.
  seq_buf.Push(chunk_view);
  const auto m = std::max_element(seq_buf_view.begin(), seq_buf_view.end());
  EXPECT_EQ(0, *m);

  // Check that the last item moves left by N positions after a push op.
  for (T i = 0; i < N; ++i)
    chunk[i] = i + 1;
  seq_buf.Push(chunk_view);
  const T last = chunk[N - 1];
  for (T i = 0; i < N; ++i)
    chunk[i] = last + i + 1;
  seq_buf.Push(chunk_view);
}

}  // namespace

// Bit-exactness check for fully connected layers.
TEST(RnnVad, CheckFullyConnectedLayerOutput) {
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

TEST(RnnVad, CheckGatedRecurrentLayer) {
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

// Runs the VAD on PCM samples and checks that the output probabilities are in a
// valid range.
TEST(RnnVad, CheckValidVadProbabilities) {
  // PCM samples reader, buffer and buffer view.
  constexpr size_t kInputAudioFrameSize = 2 * kInputFrameSize;
  BinaryFileReader<int16_t, kInputAudioFrameSize, float> samples_reader(
      test::ResourcePath("common_audio/rnn_vad/samples", "pcm"));
  std::array<float, kInputAudioFrameSize> samples;
  std::array<float, kInputFrameSize> samples_decimated;
  // Feature extractor and RNN.
  RnnVadFeaturesExtractor features_extractor;
  RnnBasedVad vad;
  // Process frames. The last one is discarded if incomplete.
  const size_t num_frames = samples_reader.data_length() / kInputFrameSize;
  for (size_t i = 0; i < num_frames; ++i) {
    samples_reader.ReadChunk({samples.data(), samples.size()});
    if (i < 20)
      continue;
    Decimate2x({samples_decimated.data(), samples_decimated.size()},
               {samples.data(), samples.size()});
    features_extractor.ComputeFeatures(
        {samples_decimated.data(), samples_decimated.size()});
    vad.ComputeVadProbability(features_extractor.GetOutput());
    EXPECT_LE(0.f, vad.vad_probability());
    EXPECT_LE(vad.vad_probability(), 1.f);
  }
}

// Bit-exactness test checking that precomputed frame-wise features lead to the
// expected VAD probabilities.
TEST(RnnVad, RnnVadBitExactness) {
  // Init.
  BinaryFileReader<float, kFeatureVectorSize> features_reader(
      test::ResourcePath("common_audio/rnn_vad/features", "out"));
  BinaryFileReader<float> vad_probs_reader(
      test::ResourcePath("common_audio/rnn_vad/vad_prob", "out"));
  const size_t number_of_frames = vad_probs_reader.data_length();
  ASSERT_EQ(features_reader.data_length(),
            number_of_frames * (kFeatureVectorSize + 1))
      << "The feature matrix and VAD probabilities vector sizes mismatch.";
  // Frame-wise buffers.
  float expected_vad_probability;
  float is_silence;
  std::array<float, kFeatureVectorSize> features;

  // Compute VAD probability using the precomputed features.
  RnnBasedVad vad;
  for (size_t i = 0; i < number_of_frames; ++i) {
    SCOPED_TRACE(i);
    // Read frame data.
    RTC_CHECK(vad_probs_reader.ReadValue(&expected_vad_probability));
    // The features file also includes a silence flag for each frame.
    RTC_CHECK(features_reader.ReadValue(&is_silence));
    RTC_CHECK(features_reader.ReadChunk({features.data(), features.size()}));
    // Skip silent frames.
    ASSERT_TRUE(is_silence == 0.f || is_silence == 1.f);
    if (is_silence == 1.f) {
      ASSERT_EQ(expected_vad_probability, 0.f);
      continue;
    }
    // Compute and check VAD probability.
    vad.ComputeVadProbability({features.data(), features.size()});
    EXPECT_NEAR(expected_vad_probability, vad.vad_probability(),
                kExpectNearTolerance);
  }
}

TEST(RnnVad, SequenceBufferGetters) {
  constexpr size_t buffer_size = 8;
  constexpr size_t chunk_size = 8;
  SequenceBuffer<uint8_t, buffer_size, chunk_size> seq_buf(0);
  EXPECT_EQ(buffer_size, seq_buf.size());
  EXPECT_EQ(chunk_size, seq_buf.chunks_size());
  // Test view.
  auto seq_buf_view = seq_buf.GetBufferView();
  EXPECT_EQ(0, *seq_buf_view.begin());
  EXPECT_EQ(0, *seq_buf_view.end());
  constexpr std::array<uint8_t, chunk_size> chunk = {10, 20, 30, 40,
                                                     50, 60, 70, 80};
  seq_buf.Push({chunk.data(), chunk_size});
  EXPECT_EQ(10, *seq_buf_view.begin());
  EXPECT_EQ(80, *(seq_buf_view.end() - 1));
}

TEST(RnnVad, SequenceBufferPushOps) {
  TestSequenceBufferPushOp<uint8_t, 32, 8>();   // Chunk size: 25%.
  TestSequenceBufferPushOp<uint8_t, 32, 16>();  // Chunk size: 50%.
  TestSequenceBufferPushOp<uint8_t, 32, 32>();  // Chunk size: 100%.
  TestSequenceBufferPushOp<uint8_t, 23, 7>();   // Non-integer ratio.
}

}  // namespace webrtc
