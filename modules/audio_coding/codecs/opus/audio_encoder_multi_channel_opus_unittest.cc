/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/opus/audio_encoder_multi_channel_opus.h"
// #include
// "modules/audio_coding/codecs/opus/audio_encoder_multi_channel_opus.h"

#include "test/gmock.h"
// #include "test/gtest.h"
// #include "test/testsupport/file_utils.h"

namespace webrtc {
using ::testing::NiceMock;
using ::testing::Return;

TEST(AudioEncoderMultiOpusTest, CreateManyChannels) {
  {
    const SdpAudioFormat format("multiopus", 48000, 6);
    auto config = AudioEncoderMultiChannelOpus::SdpToConfig(format);
    EXPECT_TRUE(config);
    auto encoder = AudioEncoderMultiChannelOpus::MakeAudioEncoder(*config, 105);
    EXPECT_TRUE(encoder);
    static_cast<void>(encoder);
  }

  {
    const SdpAudioFormat format("multiopus", 48000, 8);
    auto config = AudioEncoderMultiChannelOpus::SdpToConfig(format);
    EXPECT_TRUE(config);
    auto encoder = AudioEncoderMultiChannelOpus::MakeAudioEncoder(*config, 105);
    EXPECT_TRUE(encoder);
    static_cast<void>(encoder);
  }
}

}  // namespace webrtc
