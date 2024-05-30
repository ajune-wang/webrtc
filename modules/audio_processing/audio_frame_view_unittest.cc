/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/include/audio_frame_view.h"

#include "common_audio/channel_buffer.h"
#include "modules/audio_processing/audio_buffer.h"
#include "rtc_base/arraysize.h"
#include "test/gtest.h"

namespace webrtc {
TEST(AudioFrameTest, ConstructFromAudioBuffer) {
  constexpr int kSampleRateHz = 48000;
  constexpr int kNumChannels = 2;
  constexpr float kFloatConstant = 1272.f;
  constexpr float kIntConstant = 17252;
  const StreamConfig stream_config(kSampleRateHz, kNumChannels);
  AudioBuffer buffer(
      stream_config.sample_rate_hz(), stream_config.num_channels(),
      stream_config.sample_rate_hz(), stream_config.num_channels(),
      stream_config.sample_rate_hz(), stream_config.num_channels());

  AudioFrameView<float> non_const_view(buffer.channels(), buffer.num_channels(),
                                       buffer.num_frames());
  // Modification is allowed.
  non_const_view.channel(0)[0] = kFloatConstant;
  EXPECT_EQ(buffer.channels()[0][0], kFloatConstant);

  AudioFrameView<const float> const_view(
      buffer.channels(), buffer.num_channels(), buffer.num_frames());
  // Modification is not allowed.
  // const_view.channel(0)[0] = kFloatConstant;

  // Assignment is allowed.
  AudioFrameView<const float> other_const_view = non_const_view;
  static_cast<void>(other_const_view);

  // But not the other way. The following will fail:
  // non_const_view = other_const_view;

  AudioFrameView<float> non_const_float_view(
      buffer.channels(), buffer.num_channels(), buffer.num_frames());
  non_const_float_view.channel(0)[0] = kIntConstant;
  EXPECT_EQ(buffer.channels()[0][0], kIntConstant);
}

TEST(AudioFrameTest, IsContiguous) {
  {
    const StreamConfig stream_config(48000, 2);
    AudioBuffer buffer(
        stream_config.sample_rate_hz(), stream_config.num_channels(),
        stream_config.sample_rate_hz(), stream_config.num_channels(),
        stream_config.sample_rate_hz(), stream_config.num_channels());

    AudioFrameView<float> view(buffer.channels(), buffer.num_channels(),
                               buffer.num_frames());
    EXPECT_TRUE(view.IsContiguous());
  }
  {
    std::array<float, 100> array;
    size_t samples_per_channel = array.size() / 4;
    float* channel_pointers[] = {array.data(),
                                 array.data() + samples_per_channel,
                                 array.data() + samples_per_channel * 2,
                                 array.data() + samples_per_channel * 3};
    AudioFrameView<float> view(channel_pointers, arraysize(channel_pointers),
                               samples_per_channel);
    EXPECT_TRUE(view.IsContiguous());
  }
  {
    size_t samples_per_channel = 25;
    std::unique_ptr<float[]> channel1(new float[samples_per_channel]);
    std::unique_ptr<float[]> channel2(new float[samples_per_channel]);
    float* two_channels[2] = {channel1.get(), channel2.get()};
    AudioFrameView<float> view(two_channels, arraysize(two_channels),
                               samples_per_channel);
    EXPECT_FALSE(view.IsContiguous());
  }
  {
    std::array<std::array<float, 30>, 2> array;
    size_t samples_per_channel = array[0].size();
    float* channel_pointers[] = {array[0].data(), array[1].data()};
    AudioFrameView<float> view(channel_pointers, arraysize(channel_pointers),
                               samples_per_channel);
    EXPECT_TRUE(view.IsContiguous());
  }
}

TEST(AudioFrameTest, ConstructFromChannelBuffer) {
  ChannelBuffer<float> buffer(480, 2);
  AudioFrameView<float> view(buffer.channels(), buffer.num_channels(),
                             buffer.num_frames());
  EXPECT_EQ(view.num_channels(), 2);
  EXPECT_EQ(view.samples_per_channel(), 480);
  EXPECT_TRUE(view.IsContiguous());
}

TEST(AudioFrameTest, ToDeinterleavedView) {
  ChannelBuffer<float> buffer(480, 2);
  AudioFrameView<float> view(buffer.channels(), buffer.num_channels(),
                             buffer.num_frames());

  DeinterleavedView<float> non_const_view = view.ToDeinterleavedView();
  DeinterleavedView<const float> const_view =
      static_cast<const AudioFrameView<float>&>(view).ToDeinterleavedView();

  ASSERT_EQ(non_const_view.num_channels(), 2u);
  ASSERT_EQ(const_view.num_channels(), 2u);
  for (size_t i = 0; i < non_const_view.num_channels(); ++i) {
    EXPECT_EQ(non_const_view[i].data(), const_view[i].data());
    EXPECT_EQ(non_const_view[i].data(), view.channel(i).data());
  }
}

}  // namespace webrtc
