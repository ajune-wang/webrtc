/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_channels_selector/channel_content_replacer.h"

#include <algorithm>
#include <tuple>

#include "modules/audio_processing/audio_buffer.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

float GetChannelSampleValue(int channel) {
  return channel;
}

void PopulateAudioBuffer(AudioBuffer& audio_buffer) {
  for (int channel = 0; channel < static_cast<int>(audio_buffer.num_channels());
       ++channel) {
    rtc::ArrayView<float> channel_data(&audio_buffer.channels()[channel][0],
                                       audio_buffer.num_frames());
    std::fill(channel_data.begin(), channel_data.end(),
              GetChannelSampleValue(channel));
  }
}

void VerifyCrossFadedChannel(int channel_to_replace_from,
                             int channel_to_replace,
                             const AudioBuffer& audio_buffer) {
  rtc::ArrayView<const float> channel_data(
      &audio_buffer.channels_const()[channel_to_replace][0],
      audio_buffer.num_frames());

  const float sample_value_begin = GetChannelSampleValue(channel_to_replace);
  const float sample_value_end = GetChannelSampleValue(channel_to_replace_from);
  const float one_by_num_samples_per_channel = 1.0f / channel_data.size();
  for (int k = 0; k < static_cast<int>(channel_data.size()); ++k) {
    const float expected_value =
        sample_value_begin * (1.0f - k * one_by_num_samples_per_channel) +
        sample_value_end * k * one_by_num_samples_per_channel;
    EXPECT_EQ(channel_data[k], expected_value);
  }
}

void VerifyCopiedChannel(int channel_to_replace_from,
                         int channel_to_replace,
                         const AudioBuffer& audio_buffer) {
  rtc::ArrayView<const float> channel_data(
      &audio_buffer.channels_const()[channel_to_replace][0],
      audio_buffer.num_frames());

  const float expected_value = GetChannelSampleValue(channel_to_replace_from);
  for (int k = 0; k < static_cast<int>(channel_data.size()); ++k) {
    EXPECT_EQ(channel_data[k], expected_value);
  }
}

}  // namespace

class AudioContentReplacerParametrizedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, bool>> {};

INSTANTIATE_TEST_SUITE_P(
    MultiChannelMultiRate,
    AudioContentReplacerParametrizedTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(true)));

TEST_P(AudioContentReplacerParametrizedTest, CorrectEstimates) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const bool start_with_replacing_channel_0 = std::get<1>(GetParam());

  int channel_to_replace_from = start_with_replacing_channel_0 ? 1 : 0;
  int channel_to_replace = start_with_replacing_channel_0 ? 0 : 1;

  constexpr int kNumChannels = 2;
  AudioBuffer audio_buffer(sample_rate_hz, kNumChannels, sample_rate_hz,
                           kNumChannels, sample_rate_hz, kNumChannels);
  PopulateAudioBuffer(audio_buffer);

  ChannelContentReplacer replacer(channel_to_replace);
  replacer.SetAudioProperties(audio_buffer);
  replacer.Reset();

  constexpr int kNumCopyReplacementsToEvaluate = 10;

  // Verify replacement in one direction.
  replacer.ReplaceChannelContent(channel_to_replace_from, audio_buffer);
  VerifyCrossFadedChannel(channel_to_replace_from, channel_to_replace,
                          audio_buffer);

  for (int k = 0; k < kNumCopyReplacementsToEvaluate; ++k) {
    PopulateAudioBuffer(audio_buffer);
    replacer.ReplaceChannelContent(channel_to_replace_from, audio_buffer);
    VerifyCopiedChannel(channel_to_replace_from, channel_to_replace,
                        audio_buffer);
  }

  // Verify replacement in the other direction.
  channel_to_replace_from = start_with_replacing_channel_0 ? 0 : 1;
  replacer.ReplaceChannelContent(channel_to_replace_from, audio_buffer);
  for (int k = 0; k < kNumCopyReplacementsToEvaluate; ++k) {
    PopulateAudioBuffer(audio_buffer);
    replacer.ReplaceChannelContent(channel_to_replace_from, audio_buffer);
    VerifyCopiedChannel(channel_to_replace_from, channel_to_replace,
                        audio_buffer);
  }
}


}  // namespace webrtc
