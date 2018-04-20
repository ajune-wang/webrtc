/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/clock_drift_detector.h"

#include <array>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

void PoulateTemplateFilter(rtc::ArrayView<float> h_template) {
  for (size_t k = 0; k < h_template.size(); ++k) {
    h_template[k] = 0.001f * (1.f - 2.f * (k % 2));
  }
}

float DriftSamplesPerBlock(float drift_percent) {
  return drift_percent * 16000.f / kNumBlocksPerSecond;
}

int ComputePeakIndex(float drift_percent,
                     int initial_peak_index,
                     size_t max_peak_index,
                     size_t block_number) {
  float drift_samples_per_block = DriftSamplesPerBlock(drift_percent);
  int peak_index = static_cast<int>(
      initial_peak_index + block_number * drift_samples_per_block + 0.5f);
  return peak_index % (max_peak_index + 1);
}

}  // namespace

TEST(ClockDriftDetector, ContinuousUpdates) {
  constexpr int kLengthBlocks = 12;
  constexpr int kLengthSamples = kLengthBlocks * kBlockSize;
  constexpr size_t kNumBlocksToProcess = 30 * kNumBlocksPerSecond;
  std::array<float, kLengthSamples> h_template;
  std::array<float, kLengthSamples> h;
  PoulateTemplateFilter(h_template);

  ClockDriftDetector detector;

  float drift = 0.0001f;
  constexpr int kPeakStartIndex = 2;
  for (size_t k = 0; k < kNumBlocksToProcess; ++k) {
    std::copy(h_template.begin(), h_template.end(), h.begin());
    int peak_index = ComputePeakIndex(drift, kPeakStartIndex, h.size() - 1, k);
    h[peak_index] = 1;
    detector.Analyze(k, h);
  }

  EXPECT_TRUE(detector.DriftDetected());
}

}  // namespace webrtc
