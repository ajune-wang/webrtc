/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_channels_selector/dc_levels_estimator.h"

#include <math.h>

#include <algorithm>
#include <tuple>

#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

void PopulateAudioBuffer(float dc_level,
                         int& generated_sample_counter,
                         AudioBuffer& audio_buffer) {
  for (int channel = 0; channel < static_cast<int>(audio_buffer.num_channels());
       ++channel) {
    rtc::ArrayView<float> channel_data(&audio_buffer.channels()[channel][0],
                                       audio_buffer.num_frames());

    constexpr int kNumFramesPerSecond = 100;
    const float sample_rate_hz =
        audio_buffer.num_frames() * kNumFramesPerSecond;
    for (int k = 0; k < static_cast<int>(channel_data.size()); ++k) {
      ++generated_sample_counter;
      constexpr float kPi = 3.141592f;
      constexpr float kApmlitudeScaling = 1000.0f;
      constexpr float kFrequencyScalingHz = 100.0f;

      channel_data[k] = channel * kApmlitudeScaling *
                            sin(2 * kPi * channel * kFrequencyScalingHz *
                                generated_sample_counter / sample_rate_hz) +
                        dc_level;
    }
  }
}

}  // namespace

class DcLevelsEstimatorParametrizedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<float> {};

INSTANTIATE_TEST_SUITE_P(MultiDcLevels,
                         DcLevelsEstimatorParametrizedTest,
                         ::testing::Values(0.0f, -5.1f, 10.7f, 200.0f));

TEST_P(DcLevelsEstimatorParametrizedTest, VerifyEstimates) {
  const float true_dc_level = GetParam();
  DcLevelsEstimator estimator;
  for (int sample_rate_hz : {16000, 32000, 48000}) {
    for (int num_channels : {1, 2, 4, 8}) {
      int generated_sample_counter = 0;
      AudioBuffer audio_buffer(sample_rate_hz, num_channels, sample_rate_hz,
                               num_channels, sample_rate_hz, num_channels);
      PopulateAudioBuffer(true_dc_level, generated_sample_counter,
                          audio_buffer);
      estimator.SetAudioProperties(audio_buffer);
      estimator.Reset();

      int num_analyzed_frames = 0;
      bool reliable_estimate = false;
      while (!reliable_estimate) {
        reliable_estimate = estimator.Update(audio_buffer);
        ++num_analyzed_frames;
      }
      EXPECT_GE(num_analyzed_frames, 100);

      rtc::ArrayView<const float> levels = estimator.GetLevels();

      for (const float level : levels) {
        EXPECT_NEAR(level, true_dc_level,
                    std::max(fabs(true_dc_level) * 0.01f, 0.01f));
      }
    }
  }
}

}  // namespace webrtc
