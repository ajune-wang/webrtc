/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/rnn_vad/features_extraction.h"

#include "common_audio/rnn_vad/test_utils.h"
#include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

using rnn_vad::kFeatureVectorSize;
using rnn_vad::kFrameSize10ms48kHz;
using rnn_vad::RnnVadFeaturesExtractor;

namespace {

// // Compare each set of features and return true if the estimated pitch
// differs. bool CheckFeatures(rtc::ArrayView<const float> expected,
//                    rtc::ArrayView<const float> computed) {
//   // Cepstral features.
//   ExpectNearAbsolute({expected.data(), 6}, {computed.data(), 6},
//                      1e-5f);  // Average.
//   ExpectNearAbsolute({expected.data() + 6, 16}, {computed.data() + 6, 16},
//                      1e-5f);  // Higher bands.
//   ExpectNearAbsolute({expected.data() + 22, 6}, {computed.data() + 22, 6},
//                      1e-5f);  // First derivative.
//   ExpectNearAbsolute({expected.data() + 28, 6}, {computed.data() + 28, 6},
//                      1e-5f);                       // Second derivative.
//   EXPECT_NEAR(expected[41], computed[41], 1e-5f);  // Spectral variability.

//   auto pitch_period = [](float x) {
//     return static_cast<size_t>(x * 100 + 300);
//   };
//   const size_t expected_pitch_period = pitch_period(expected[40]);
//   const size_t computed_pitch_period = pitch_period(computed[40]);
//   // EXPECT_EQ(expected_pitch_period, computed_pitch_period);
//   // Estimated pitch may differ.
//   if (expected_pitch_period == computed_pitch_period) {
//     ExpectNearAbsolute({expected.data() + 34, 6}, {computed.data() + 34, 6},
//                        1e-5f);  // Band correlations.
//     return true;
//   }
//   return false;
// }

}  // namespace

// // Check that the RNN VAD features difference between (i) those computed
// // using the porting and (ii) those computed using the reference code is
// within
// // a tolerance.
// TEST(RnnVadTest, DISABLED_CheckExtractedFeaturesWithinTolerance) {
//   // PCM samples reader and buffers.
//   auto samples_reader = CreatePcmSamplesReader(kFrameSize10ms48kHz);
//   const size_t num_frames = samples_reader.second;
//   std::array<float, kFrameSize10ms48kHz> samples;
//   rtc::ArrayView<float, kFrameSize10ms48kHz> samples_view(samples.data(),
//                                                           kFrameSize10ms48kHz);
//   // Read ground-truth.
//   auto features_reader = CreateFeatureMatrixReader();
//   ASSERT_EQ(features_reader.second, num_frames);
//   std::array<float, kFeatureVectorSize> expected_features;
//   rtc::ArrayView<float, kFeatureVectorSize> expected_features_view(
//       expected_features.data(), expected_features.size());
//   float expected_is_silence;
//   // Init pipeline.
//   RnnVadFeaturesExtractor features_extractor;
//   auto computed_features_view = features_extractor.GetFeatureVectorView();
//   // Process frames.
//   size_t num_estimated_pitch_diffs = 0;
//   {
//     FloatingPointExceptionObserver fpe_observer;

//     for (size_t i = 0; i < num_frames; ++i) {
//       SCOPED_TRACE(i);
//       // Read ground-truth.
//       features_reader.first->ReadValue(&expected_is_silence);
//       features_reader.first->ReadChunk(expected_features_view);
//       // Read 10 ms audio frame and compute features.
//       samples_reader.first->ReadChunk(samples_view);
//       const bool is_silence =
//           features_extractor.ComputeFeaturesCheckSilence(samples_view);
//       ASSERT_EQ(expected_is_silence == 1.f, is_silence);
//       if (is_silence)
//         continue;
//       // Compare features.
//       if (CheckFeatures(expected_features_view, computed_features_view)) {
//         num_estimated_pitch_diffs++;
//       }
//     }
//   }
//   // Maximum pitch differences: 5%.
//   EXPECT_LE(num_estimated_pitch_diffs, static_cast<size_t>(0.05f *
//   num_frames));
// }

}  // namespace test
}  // namespace webrtc
