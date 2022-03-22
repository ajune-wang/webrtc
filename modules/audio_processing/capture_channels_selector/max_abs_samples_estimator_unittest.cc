/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_channels_selector/max_abs_samples_estimator.h"

#include <algorithm>
#include <tuple>

#include "api/array_view.h"
#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

float GetChannelSampleAmplitude(int channel) {
  return channel;
}

void PopulateAudioBuffer(float dc_level, AudioBuffer& audio_buffer) {
  for (int channel = 0; channel < static_cast<int>(audio_buffer.num_channels());
       ++channel) {
    rtc::ArrayView<float> channel_data(&audio_buffer.channels()[channel][0],
                                       audio_buffer.num_frames());
    for (int k = 0; k < static_cast<int>(channel_data.size()); ++k) {
      channel_data[k] = GetChannelSampleAmplitude(channel) * (k % 2 == 0 ? 1 : -1) + dc_level;
    }
  }
}

void VerifyMaxAbsSamplesValues(const std::vector<float>& max_abs_samples) {
  for (int channel = 0; channel < static_cast<int>(max_abs_samples.size());
       ++channel) {
      EXPECT_EQ(max_abs_samples[channel], GetChannelSampleAmplitude(channel));
  }
}

}  // namespace

class MaxAbsSamplesEstimatorParametrizedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<float> {};

INSTANTIATE_TEST_SUITE_P(MultiDcLevels,
                         MaxAbsSamplesEstimatorParametrizedTest,
                         ::testing::Values(0.0f, -5.1f, 10.7f));

TEST_P(MaxAbsSamplesEstimatorParametrizedTest, VerifyEstimates) {
  const float dc_level = GetParam();
  MaxAbsSamplesEstimator estimator;
  for (int sample_rate_hz : {16000, 32000, 48000}) {
    for (int num_channels : {1, 2, 4}) {
            const std::vector<float> dc_levels(num_channels, dc_level);
      AudioBuffer audio_buffer(sample_rate_hz, num_channels, sample_rate_hz,
                               num_channels, sample_rate_hz, num_channels);
      PopulateAudioBuffer(dc_level, audio_buffer);
      estimator.SetAudioProperties(audio_buffer);
      estimator.Reset();

      constexpr int kNumFramesToAnalyze = 2000;
      for (int k = 0; k < kNumFramesToAnalyze; ++k) {
        estimator.Update(audio_buffer, dc_levels);
      }

      const std::vector<float>& max_abs_samples =
          estimator.GetMaxAbsSampleInChannels();
      EXPECT_EQ(max_abs_samples.size(), static_cast<size_t>(num_channels));
      VerifyMaxAbsSamplesValues(max_abs_samples);
    }
  }
}

}  // namespace webrtc
