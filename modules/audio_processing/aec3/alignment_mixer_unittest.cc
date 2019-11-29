/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/alignment_mixer.h"

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::AllOf;
using ::testing::Each;

namespace webrtc {
namespace {}  // namespace

TEST(AlignmentMixer, GeneralAdaptiveMode) {
  constexpr float kStrongestSignalScaling = 10.f;
  for (int num_channels = 2; num_channels < 8; ++num_channels) {
    for (int strongest_ch = 2; strongest_ch < num_channels; ++strongest_ch) {
      AlignmentMixer am(num_channels, /*downmix*/ false,
                        /*adaptive_selection*/ false, /*excitation_limit*/ 1.f,
                        /*prefer_first_two_channels*/ false);

      std::vector<std::vector<float>> x(num_channels,
                                        std::vector<float>(kBlockSize, 0.f));
      const auto channel_value = [](int frame_index, int channel_index) {
        return static_cast<float>(frame_index + channel_index);
      };
      for (int frame = 0; frame < 10; ++frame) {
        for (int ch = 0; ch < num_channels; ++ch) {
          float scaling = ch == strongest_ch ? kStrongestSignalScaling : 1.f;
          std::fill(x[ch].begin(), x[ch].end(),
                    channel_value(frame, ch) * scaling);
        }

        std::array<float, kBlockSize> y;
        y.fill(-1.f);
        am.ProduceOutput(x, y);

        float expected_mixed_value = 0.f;
        for (int ch = 0; ch < num_channels; ++ch) {
          expected_mixed_value += channel_value(frame, ch);
        }
        expected_mixed_value *= 1.f / num_channels;

        EXPECT_THAT(y, AllOf(Each(expected_mixed_value)));
      }
    }
  }
}

TEST(AlignmentMixer, DownmixMode) {
  for (int num_channels = 1; num_channels < 8; ++num_channels) {
    AlignmentMixer am(num_channels, /*downmix*/ true,
                      /*adaptive_selection*/ false, /*excitation_limit*/ 1.f,
                      /*prefer_first_two_channels*/ false);

    std::vector<std::vector<float>> x(num_channels,
                                      std::vector<float>(kBlockSize, 0.f));
    const auto channel_value = [](int frame_index, int channel_index) {
      return static_cast<float>(frame_index + channel_index);
    };
    for (int frame = 0; frame < 10; ++frame) {
      for (int ch = 0; ch < num_channels; ++ch) {
        std::fill(x[ch].begin(), x[ch].end(), channel_value(frame, ch));
      }

      std::array<float, kBlockSize> y;
      y.fill(-1.f);
      am.ProduceOutput(x, y);

      float expected_mixed_value = 0.f;
      for (int ch = 0; ch < num_channels; ++ch) {
        expected_mixed_value += channel_value(frame, ch);
      }
      expected_mixed_value *= 1.f / num_channels;

      EXPECT_THAT(y, AllOf(Each(expected_mixed_value)));
    }
  }
}

TEST(AlignmentMixer, FixedMode) {
  for (int num_channels = 1; num_channels < 8; ++num_channels) {
    AlignmentMixer am(num_channels, /*downmix*/ false,
                      /*adaptive_selection*/ false, /*excitation_limit*/ 1.f,
                      /*prefer_first_two_channels*/ false);

    std::vector<std::vector<float>> x(num_channels,
                                      std::vector<float>(kBlockSize, 0.f));
    const auto channel_value = [](int frame_index, int channel_index) {
      return static_cast<float>(frame_index + channel_index);
    };
    for (int frame = 0; frame < 10; ++frame) {
      for (int ch = 0; ch < num_channels; ++ch) {
        std::fill(x[ch].begin(), x[ch].end(), channel_value(frame, ch));
      }

      std::array<float, kBlockSize> y;
      y.fill(-1.f);
      am.ProduceOutput(x, y);
      EXPECT_THAT(y, AllOf(Each(channel_value(frame, 0))));
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(AlignmentMixer, NegativeNumChannels) {
  EXPECT_DEATH(
      AlignmentMixer(/*num_channels*/ -1, /*downmix*/ false,
                     /*adaptive_selection*/ false, /*excitation_limit*/ 1.f,
                     /*prefer_first_two_channels*/ false);
      , "");
}

TEST(AlignmentMixer, ZeroNumChannels) {
  EXPECT_DEATH(
      AlignmentMixer(/*num_channels*/ 0, /*downmix*/ false,
                     /*adaptive_selection*/ false, /*excitation_limit*/ 1.f,
                     /*prefer_first_two_channels*/ false);
      , "");
}

TEST(AlignmentMixer, IncorrectVariant) {
  EXPECT_DEATH(
      AlignmentMixer(/*num_channels*/ 1, /*downmix*/ true,
                     /*adaptive_selection*/ true, /*excitation_limit*/ 1.f,
                     /*prefer_first_two_channels*/ false);
      , "");
}

#endif

}  // namespace webrtc
