/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/audio_encoder_factory_template.h"

#include <memory>

#include "api/audio_codecs/L16/audio_encoder_L16.h"
#include "api/audio_codecs/g711/audio_encoder_g711.h"
#include "api/audio_codecs/g722/audio_encoder_g722.h"
#include "api/audio_codecs/ilbc/audio_encoder_ilbc.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_audio_encoder.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsNull;
using ::testing::Pointer;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;

struct BogusParams {
  static SdpAudioFormat AudioFormat() { return {"bogus", 8000, 1}; }
  static AudioCodecInfo CodecInfo() { return {8000, 1, 12345}; }
};

struct ShamParams {
  static SdpAudioFormat AudioFormat() {
    return {"sham", 16000, 2, {{"param", "value"}}};
  }
  static AudioCodecInfo CodecInfo() { return {16000, 2, 23456}; }
};

template <typename Params>
struct AudioEncoderFakeApi {
  struct Config {
    SdpAudioFormat audio_format;
  };

  static absl::optional<Config> SdpToConfig(
      const SdpAudioFormat& audio_format) {
    if (Params::AudioFormat() == audio_format) {
      return {{audio_format}};
    } else {
      return absl::nullopt;
    }
  }

  static void AppendSupportedEncoders(std::vector<AudioCodecSpec>* specs) {
    specs->push_back({Params::AudioFormat(), Params::CodecInfo()});
  }

  static AudioCodecInfo QueryAudioEncoder(const Config&) {
    return Params::CodecInfo();
  }

  static std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      const Config&,
      int payload_type,
      absl::optional<AudioCodecPairId> /*codec_pair_id*/) {
    auto enc = std::make_unique<StrictMock<MockAudioEncoder>>();
    EXPECT_CALL(*enc, SampleRateHz)
        .WillOnce(Return(Params::CodecInfo().sample_rate_hz));
    return enc;
  }
};

TEST(AudioEncoderFactoryTemplateTest, OneEncoderType) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderFakeApi<BogusParams>>();
  EXPECT_THAT(
      factory->GetSupportedEncoders(),
      ElementsAre(AudioCodecSpec{{"bogus", 8000, 1}, {8000, 1, 12345}}));
  EXPECT_EQ(factory->QueryAudioEncoder({"foo", 8000, 1}), absl::nullopt);
  EXPECT_EQ(factory->QueryAudioEncoder({"bogus", 8000, 1}),
            AudioCodecInfo(8000, 1, 12345));
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"bar", 16000, 1}, absl::nullopt),
              IsNull());
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"bogus", 8000, 1}, absl::nullopt),
              Pointer(Property(&AudioEncoder::SampleRateHz, 8000)));
}

TEST(AudioEncoderFactoryTemplateTest, TwoEncoderTypes) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderFakeApi<BogusParams>,
                                           AudioEncoderFakeApi<ShamParams>>();
  EXPECT_THAT(
      factory->GetSupportedEncoders(),
      ElementsAre(AudioCodecSpec{{"bogus", 8000, 1}, {8000, 1, 12345}},
                  AudioCodecSpec{{"sham", 16000, 2, {{"param", "value"}}},
                                 {16000, 2, 23456}}));
  EXPECT_EQ(factory->QueryAudioEncoder({"foo", 8000, 1}), absl::nullopt);
  EXPECT_EQ(factory->QueryAudioEncoder({"bogus", 8000, 1}),
            AudioCodecInfo(8000, 1, 12345));
  EXPECT_EQ(
      factory->QueryAudioEncoder({"sham", 16000, 2, {{"param", "value"}}}),
      AudioCodecInfo(16000, 2, 23456));
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"bar", 16000, 1}, absl::nullopt),
              IsNull());
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"bogus", 8000, 1}, absl::nullopt),
              Pointer(Property(&AudioEncoder::SampleRateHz, 8000)));

  EXPECT_THAT(factory->MakeAudioEncoder(17, {"sham", 16000, 2}, absl::nullopt),
              IsNull());
  EXPECT_THAT(factory->MakeAudioEncoder(
                  17, {"sham", 16000, 2, {{"param", "value"}}}, absl::nullopt),
              Pointer(Property(&AudioEncoder::SampleRateHz, 16000)));
}

TEST(AudioEncoderFactoryTemplateTest, G711) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderG711>();
  EXPECT_THAT(factory->GetSupportedEncoders(),
              ElementsAre(AudioCodecSpec{{"PCMU", 8000, 1}, {8000, 1, 64000}},
                          AudioCodecSpec{{"PCMA", 8000, 1}, {8000, 1, 64000}}));
  EXPECT_EQ(factory->QueryAudioEncoder({"PCMA", 16000, 1}), absl::nullopt);
  EXPECT_EQ(factory->QueryAudioEncoder({"PCMA", 8000, 1}),
            AudioCodecInfo(8000, 1, 64000));
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"PCMU", 16000, 1}, absl::nullopt),
              IsNull());
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"PCMU", 8000, 1}, absl::nullopt),
              Pointer(Property(&AudioEncoder::SampleRateHz, 8000)));

  EXPECT_THAT(factory->MakeAudioEncoder(17, {"PCMA", 8000, 1}, absl::nullopt),
              Pointer(Property(&AudioEncoder::SampleRateHz, 8000)));
}

TEST(AudioEncoderFactoryTemplateTest, G722) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderG722>();
  EXPECT_THAT(
      factory->GetSupportedEncoders(),
      ElementsAre(AudioCodecSpec{{"G722", 8000, 1}, {16000, 1, 64000}}));
  EXPECT_EQ(factory->QueryAudioEncoder({"foo", 8000, 1}), absl::nullopt);
  EXPECT_EQ(factory->QueryAudioEncoder({"G722", 8000, 1}),
            AudioCodecInfo(16000, 1, 64000));
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"bar", 16000, 1}, absl::nullopt),
              IsNull());
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"G722", 8000, 1}, absl::nullopt),
              Pointer(Property(&AudioEncoder::SampleRateHz, 16000)));
}

TEST(AudioEncoderFactoryTemplateTest, Ilbc) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderIlbc>();
  EXPECT_THAT(factory->GetSupportedEncoders(),
              ElementsAre(AudioCodecSpec{{"ILBC", 8000, 1}, {8000, 1, 13333}}));
  EXPECT_EQ(factory->QueryAudioEncoder({"foo", 8000, 1}), absl::nullopt);
  EXPECT_EQ(factory->QueryAudioEncoder({"ilbc", 8000, 1}),
            AudioCodecInfo(8000, 1, 13333));
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"bar", 8000, 1}, absl::nullopt),
              IsNull());
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"ilbc", 8000, 1}, absl::nullopt),
              Pointer(Property(&AudioEncoder::SampleRateHz, 8000)));
}

TEST(AudioEncoderFactoryTemplateTest, L16) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderL16>();
  EXPECT_THAT(
      factory->GetSupportedEncoders(),
      ElementsAre(
          AudioCodecSpec{{"L16", 8000, 1}, {8000, 1, 8000 * 16}},
          AudioCodecSpec{{"L16", 16000, 1}, {16000, 1, 16000 * 16}},
          AudioCodecSpec{{"L16", 32000, 1}, {32000, 1, 32000 * 16}},
          AudioCodecSpec{{"L16", 8000, 2}, {8000, 2, 8000 * 16 * 2}},
          AudioCodecSpec{{"L16", 16000, 2}, {16000, 2, 16000 * 16 * 2}},
          AudioCodecSpec{{"L16", 32000, 2}, {32000, 2, 32000 * 16 * 2}}));
  EXPECT_EQ(factory->QueryAudioEncoder({"L16", 8000, 0}), absl::nullopt);
  EXPECT_EQ(factory->QueryAudioEncoder({"L16", 48000, 1}),
            AudioCodecInfo(48000, 1, 48000 * 16));
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"L16", 8000, 0}, absl::nullopt),
              IsNull());
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"L16", 48000, 2}, absl::nullopt),
              Pointer(Property(&AudioEncoder::SampleRateHz, 48000)));
}

TEST(AudioEncoderFactoryTemplateTest, Opus) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderOpus>();
  AudioCodecInfo info = {48000, 1, 32000, 6000, 510000};
  info.allow_comfort_noise = false;
  info.supports_network_adaption = true;
  EXPECT_THAT(
      factory->GetSupportedEncoders(),
      ElementsAre(AudioCodecSpec{
          {"opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}},
          info}));
  EXPECT_EQ(factory->QueryAudioEncoder({"foo", 8000, 1}), absl::nullopt);
  EXPECT_EQ(
      factory->QueryAudioEncoder(
          {"opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}}),
      info);
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"bar", 16000, 1}, absl::nullopt),
              IsNull());
  EXPECT_THAT(factory->MakeAudioEncoder(17, {"opus", 48000, 2}, absl::nullopt),
              Pointer(Property(&AudioEncoder::SampleRateHz, 48000)));
}

}  // namespace
}  // namespace webrtc
