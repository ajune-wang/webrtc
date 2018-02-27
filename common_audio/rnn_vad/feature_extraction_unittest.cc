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
#include <string>

#include "common_audio/rnn_vad/downsample.h"
#include "common_audio/rnn_vad/features_extraction.h"
#include "common_audio/rnn_vad/test_utils.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

using rnn_vad::Decimate48k24k;
using rnn_vad::kInputFrameSize;
using rnn_vad::kFeatureVectorSize;
using rnn_vad::RnnVadFeaturesExtractor;

namespace {

constexpr double kExpectNearTolerance = 1e-6;

void ExpectNear(rtc::ArrayView<const float> a, rtc::ArrayView<const float> b) {
  EXPECT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    std::ostringstream ss;
    ss << "feature item " << i;
    SCOPED_TRACE(ss.str());
    EXPECT_NEAR(a[i], b[i], kExpectNearTolerance);
  }
}

}  // namespace

// TODO(alessiob): Enable once feature extraction is fully implemented.
TEST(RnnVad, DISABLED_FeaturesExtractorBitExactness) {
  // PCM samples reader and buffers.
  constexpr size_t kInputAudioFrameSize = 2 * kInputFrameSize;
  BinaryFileReader<int16_t, kInputAudioFrameSize, float> samples_reader(
      test::ResourcePath("common_audio/rnn_vad/samples", "pcm"));
  std::array<float, kInputAudioFrameSize> samples;
  std::array<float, kInputFrameSize> samples_decimated;
  // Features reader and buffers.
  BinaryFileReader<float, kFeatureVectorSize> features_reader(
      test::ResourcePath("common_audio/rnn_vad/features", "out"));
  float is_silence;
  std::array<float, kFeatureVectorSize> features;
  rtc::ArrayView<const float, kFeatureVectorSize> expected_features_view(
      features.data(), features.size());
  // Feature extractor.
  RnnVadFeaturesExtractor features_extractor;
  auto extracted_features_view = features_extractor.GetOutput();
  // Process frames. The last one is discarded if incomplete.
  const size_t num_frames = samples_reader.data_length() / kInputFrameSize;
  for (size_t i = 0; i < num_frames; ++i) {
    std::ostringstream ss;
    ss << "frame " << i;
    SCOPED_TRACE(ss.str());
    // Read and downsample audio frame.
    samples_reader.ReadChunk({samples.data(), samples.size()});
    Decimate48k24k({samples_decimated.data(), samples_decimated.size()},
                   {samples.data(), samples.size()});
    // Compute feature vector.
    features_extractor.ComputeFeatures(
        {samples_decimated.data(), samples_decimated.size()});
    // Read expected feature vector.
    RTC_CHECK(features_reader.ReadValue(&is_silence));
    RTC_CHECK(features_reader.ReadChunk({features.data(), features.size()}));
    // Check silence flag and feature vector.
    EXPECT_EQ(is_silence, features_extractor.is_silence());
    ExpectNear(expected_features_view, extracted_features_view);
  }
}

}  // namespace test
}  // namespace webrtc
