/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "api/voip/voip_engine_builder.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "rtc_base/logging.h"
#include "test/gtest.h"
#include "test/mock_audio_decoder_factory.h"
#include "test/mock_audio_encoder_factory.h"

namespace webrtc {

TEST(VoipEngineBuilderTest, EngineWithMockModules) {
  auto encoder_factory = new rtc::RefCountedObject<MockAudioEncoderFactory>();
  auto decoder_factory = new rtc::RefCountedObject<MockAudioDecoderFactory>();
  auto audio_device = test::MockAudioDeviceModule::CreateNice();
  std::unique_ptr<AudioProcessing> audio_processing(
      new rtc::RefCountedObject<test::MockAudioProcessing>());
  auto voip_engine = webrtc::VoipEngineBuilder()
                         .SetAudioEncoderFactory(encoder_factory)
                         .SetAudioDecoderFactory(decoder_factory)
                         .SetAudioDeviceModule(audio_device)
                         .SetAudioProcessing(std::move(audio_processing))
                         .Create();
  EXPECT_NE(voip_engine, nullptr);
}

TEST(VoipEngineBuilderTest, ExpectFailureWithoutEncoderFactory) {
  auto decoder_factory = new rtc::RefCountedObject<MockAudioDecoderFactory>();
  auto audio_device = test::MockAudioDeviceModule::CreateNice();
  std::unique_ptr<AudioProcessing> audio_processing(
      new rtc::RefCountedObject<test::MockAudioProcessing>());
  auto voip_engine = webrtc::VoipEngineBuilder()
                         .SetAudioDecoderFactory(decoder_factory)
                         .SetAudioDeviceModule(audio_device)
                         .SetAudioProcessing(std::move(audio_processing))
                         .Create();
  EXPECT_EQ(voip_engine, nullptr);
}

TEST(VoipEngineBuilderTest, ExpectFailureWithoutDecoderFactory) {
  auto encoder_factory = new rtc::RefCountedObject<MockAudioEncoderFactory>();
  auto audio_device = test::MockAudioDeviceModule::CreateNice();
  std::unique_ptr<AudioProcessing> audio_processing(
      new rtc::RefCountedObject<test::MockAudioProcessing>());
  auto voip_engine = webrtc::VoipEngineBuilder()
                         .SetAudioEncoderFactory(encoder_factory)
                         .SetAudioDeviceModule(audio_device)
                         .SetAudioProcessing(std::move(audio_processing))
                         .Create();
  EXPECT_EQ(voip_engine, nullptr);
}

}  // namespace webrtc
