/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_channels_selector/audio_content_analyzer.h"

#include <algorithm>
#include <tuple>

#include "api/array_view.h"
#include "modules/audio_processing/audio_buffer.h"
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

void VerifyMaxAbsSamplesValues(bool use_strict_threshold,
                               const std::vector<float>& max_abs_samples) {
  for (int channel = 0; channel < static_cast<int>(max_abs_samples.size());
       ++channel) {
    if (use_strict_threshold) {
      EXPECT_EQ(max_abs_samples[channel], GetChannelSampleAmplitude(channel));
    } else {
      constexpr float kTolerance = 0.1f;
      EXPECT_NEAR(max_abs_samples[channel], GetChannelSampleAmplitude(channel), kTolerance);
    }
  }
}

void VerifyEnergyValues(const std::vector<float>& energies,
                        int num_samples_per_channel) {
  for (int channel = 0; channel < static_cast<int>(energies.size());
       ++channel) {
    constexpr int kTolerance = 1;
    const float expected_amplitude = GetChannelSampleAmplitude(channel);
    EXPECT_NEAR(energies[channel], expected_amplitude * expected_amplitude * num_samples_per_channel,
                kTolerance);
  }
}

}  // namespace

class AudioContentAnalyzerParametrizedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<float> {};

INSTANTIATE_TEST_SUITE_P(MultiDcLevels,
                         AudioContentAnalyzerParametrizedTest,
                         ::testing::Values(0.0f, -5.1f, 10.7f));

TEST_P(AudioContentAnalyzerParametrizedTest, VerifyEstimates) {
  const float dc_level = GetParam();
  AudioContentAnalyzer analyzer;
  for (int sample_rate_hz : {16000, 32000, 48000}) {
    for (int num_channels : {1, 2, 4}) {
      AudioBuffer audio_buffer(sample_rate_hz, num_channels, sample_rate_hz,
                               num_channels, sample_rate_hz, num_channels);
      PopulateAudioBuffer(dc_level, audio_buffer);
      analyzer.SetAudioProperties(audio_buffer);
      analyzer.Reset();

      constexpr int kNumFramesToAnalyze = 2000;
      bool reliable_estimates = false;
      for (int k = 0; k < kNumFramesToAnalyze; ++k) {
        reliable_estimates = analyzer.Analyze(audio_buffer);
      }
      EXPECT_TRUE(reliable_estimates);

      const std::vector<float>& max_abs_samples =
          analyzer.GetMaxAbsSampleInChannels();
      EXPECT_EQ(max_abs_samples.size(), static_cast<size_t>(num_channels));
      // Verify using a DC-level dependent tolerance to take into account the
      // errors in the DC-level compensation.
      VerifyMaxAbsSamplesValues(dc_level == 0.0f, max_abs_samples);

      const std::vector<float>& energies = analyzer.GetChannelEnergies();
      EXPECT_EQ(energies.size(), static_cast<size_t>(num_channels));
      VerifyEnergyValues(energies, audio_buffer.num_frames());
    }
  }
}

}  // namespace webrtc
