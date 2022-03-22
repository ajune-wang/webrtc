/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_channels_selector/mono_channel_selector.h"

#include <algorithm>
#include <tuple>

#include "api/array_view.h"
#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

void PopulateAudioBuffer(bool add_dc_levels, rtc::ArrayView<const float> dc_levels, rtc::ArrayView<const float> channel_amplitudes, AudioBuffer& audio_buffer) {
  for (int channel = 0; channel < static_cast<int>(audio_buffer.num_channels());
       ++channel) {
    rtc::ArrayView<float> channel_data(&audio_buffer.channels()[channel][0],
                                       audio_buffer.num_frames());
    for (int k = 0; k < static_cast<int>(channel_data.size()); ++k) {
      channel_data[k] = channel_amplitudes[channel] * (k % 2 == 0 ? 1 : -1);
    }
    if (add_dc_levels) {
        for (int k = 0; k < static_cast<int>(channel_data.size()); ++k) {
      channel_data[k] +=dc_levels[channel];
    }
    }
  }
}

void CompareChannelData(rtc::ArrayView<const float> desired_channel_data, rtc::ArrayView<const float> output_channel_data) {
  ASSERT_EQ(desired_channel_data.size(),output_channel_data.size());
  for (int k = 0; k < static_cast<int>(desired_channel_data.size()); ++k) {
    EXPECT_EQ(desired_channel_data[k], output_channel_data[k]);
  }
}

}  // namespace

class MonoChannelSelectorParametrizedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool,int>> {};

INSTANTIATE_TEST_SUITE_P(
    MultiChannelMultiRate,
    MonoChannelSelectorParametrizedTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(false,true),
                                              ::testing::Values(false,true),
                       ::testing::Values(2, 4, 8)));

TEST_P(MonoChannelSelectorParametrizedTest, ReplaceZeroChannel) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int test_with_dc_levels = std::get<1>(GetParam());
  const int use_entirely_silent_poor_channel = std::get<2>(GetParam());
  const int num_channels =  std::get<3>(GetParam());


  for (int poor_channel_index = 0; poor_channel_index < num_channels; ++poor_channel_index) {
    constexpr float kDcLevels[] = {10.0f, -5.0f, 100.0f, 0.0f, 1.0f, -15.1f, 50.0f, -200.0f};
    std::vector<float> amplitudes({40.0f, 50.0f,100.0f, 200.0f,400.0f, 800.0f , 1000.0f, 1200.0f});
    if (use_entirely_silent_poor_channel) {
    amplitudes[poor_channel_index] = 0.0f;
    }
    else {
    amplitudes[poor_channel_index] = 2.0f;
    }


  MonoChannelSelector mono_channel_selector;
  constexpr int kNumFramesToAnalyze = 2000;
  for (int k = 0; k < kNumFramesToAnalyze; ++k) {
    AudioBuffer audio_buffer(sample_rate_hz, num_channels, sample_rate_hz,
                             num_channels, sample_rate_hz, num_channels);
    PopulateAudioBuffer(test_with_dc_levels, kDcLevels, amplitudes, audio_buffer);

    mono_channel_selector.DownMixToBestChannel(audio_buffer);
    EXPECT_EQ(audio_buffer.num_channels(), 1u);
  }

  AudioBuffer reference_audio_buffer(sample_rate_hz, num_channels, sample_rate_hz,
                                     num_channels, sample_rate_hz, num_channels);
  AudioBuffer audio_buffer(sample_rate_hz, num_channels, sample_rate_hz,
                           num_channels, sample_rate_hz, num_channels);
      PopulateAudioBuffer(test_with_dc_levels, kDcLevels, amplitudes, reference_audio_buffer);
          PopulateAudioBuffer(test_with_dc_levels, kDcLevels, amplitudes, audio_buffer);
  mono_channel_selector.DownMixToBestChannel(audio_buffer);
      EXPECT_EQ(audio_buffer.num_channels(), 1u);


  std::vector<float>::iterator best_amplitude_iterator = std::max_element(amplitudes.begin(), amplitudes.end());
  int best_amplitude_channel = std::distance(amplitudes.begin(), best_amplitude_iterator);

   rtc::ArrayView<const float> desired_channel_data(
       &reference_audio_buffer.channels_const()[best_amplitude_channel][0],
       audio_buffer.num_frames());
   rtc::ArrayView<const float> output_channel_data(
       &audio_buffer.channels_const()[0][0],
       audio_buffer.num_frames());

   CompareChannelData(desired_channel_data, output_channel_data);

  }
}

}  // namespace webrtc
