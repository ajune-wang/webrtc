/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/opus/audio_decoder_opus.h"

#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/opus/audio_encoder_opus_config.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "common_audio/wav_file.h"
#include "modules/audio_coding/codecs/opus/audio_encoder_opus.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using DecodeResult = ::webrtc::AudioDecoder::EncodedAudioFrame::DecodeResult;
using ParseResult = ::webrtc::AudioDecoder::ParseResult;

constexpr int kSampleRateHz = 48000;

constexpr int kInputFrameDurationMs = 10;
constexpr int kInputFrameLength = kInputFrameDurationMs * kSampleRateHz / 1000;

constexpr int kEncoderFrameDurationMs = 20;
constexpr int kEncoderFrameLength =
    kEncoderFrameDurationMs * kSampleRateHz / 1000;

AudioEncoderOpusConfig GetEncoderConfig(int num_channels, bool dtx_enabled) {
  AudioEncoderOpusConfig config;

  config.frame_size_ms = kEncoderFrameDurationMs;
  config.sample_rate_hz = kSampleRateHz;
  config.num_channels = num_channels;
  config.application = AudioEncoderOpusConfig::ApplicationMode::kVoip;
  config.bitrate_bps = std::make_optional<int>(32000);
  config.fec_enabled = false;
  config.cbr_enabled = false;
  config.max_playback_rate_hz = kSampleRateHz;
  config.complexity = 10;
  config.dtx_enabled = dtx_enabled;

  return config;
}

class WhiteNoiseGenerator {
 public:
  explicit WhiteNoiseGenerator(double amplitude_dbfs)
      : amplitude_(
            rtc::saturated_cast<int16_t>(std::pow(10, amplitude_dbfs / 20) *
                                         std::numeric_limits<int16_t>::max())),
        random_generator_(42) {}

  void GenerateNextFrame(std::array<int16_t, kInputFrameLength>& frame) {
    for (size_t i = 0; i < kInputFrameLength; ++i) {
      frame[i] = rtc::saturated_cast<int16_t>(
          random_generator_.Rand(-amplitude_, amplitude_));
    }
  }

 private:
  const int32_t amplitude_;
  Random random_generator_;
};

bool IsZeroedFrame(rtc::ArrayView<const int16_t> audio) {
  for (const int16_t& v : audio) {
    if (v != 0)
      return false;
  }
  return true;
}

bool IsTrivialStereo(rtc::ArrayView<const int16_t> audio) {
  const int num_samples =
      rtc::CheckedDivExact(audio.size(), static_cast<size_t>(2));
  for (int i = 0, j = 0; i < num_samples; ++i, j += 2) {
    if (audio[j] != audio[j + 1]) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST(AudioDecoderOpusTest, MonoEncoderStereoDecoderOutputsTrivialStereo) {
  const Environment env = EnvironmentFactory().Create();
  WhiteNoiseGenerator generator(/*amplitude_dbfs=*/-70.0);
  std::array<int16_t, kInputFrameLength> input_frame;
  // Create a mono encoder.
  const AudioEncoderOpusConfig encoder_config =
      GetEncoderConfig(/*num_channels=*/1, /*dtx_enabled=*/false);
  AudioEncoderOpusImpl encoder(env, encoder_config, 111);
  // Create a stereo decoder.
  constexpr size_t kDecoderNumChannels = 2;
  AudioDecoderOpusImpl decoder(env.field_trials(), kDecoderNumChannels,
                               kSampleRateHz);
  std::array<int16_t, kEncoderFrameLength * kDecoderNumChannels> decoded_frame;

  uint32_t rtp_timestamp = 0xFFFu;
  uint32_t timestamp = 0;
  for (int i = 0; i < 30; ++i) {
    generator.GenerateNextFrame(input_frame);
    rtc::Buffer payload;
    encoder.Encode(rtp_timestamp++, input_frame, &payload);
    if (payload.size() == 0) {
      continue;
    }

    // Decode.
    std::vector<ParseResult> parse_results =
        decoder.ParsePayload(std::move(payload), timestamp++);
    RTC_CHECK_EQ(parse_results.size(), 1);
    std::optional<DecodeResult> decode_results =
        parse_results[0].frame->Decode(decoded_frame);
    RTC_CHECK(decode_results);
    RTC_CHECK_EQ(decode_results->num_decoded_samples, decoded_frame.size());

    EXPECT_TRUE(IsTrivialStereo(decoded_frame));
  }
}

TEST(AudioDecoderOpusTest, MonoEncoderStereoDecoderOutputsNonTrivialStereoDtx) {
  const Environment env = EnvironmentFactory().Create();
  WhiteNoiseGenerator generator(/*amplitude_dbfs=*/-70.0);
  std::array<int16_t, kInputFrameLength> input_frame;
  // Create a mono encoder.
  const AudioEncoderOpusConfig encoder_config =
      GetEncoderConfig(/*num_channels=*/1, /*dtx_enabled=*/true);
  AudioEncoderOpusImpl encoder(env, encoder_config, 111);
  // Create a stereo decoder.
  constexpr size_t kDecoderNumChannels = 2;
  AudioDecoderOpusImpl decoder(env.field_trials(), kDecoderNumChannels,
                               kSampleRateHz);

  // Feed the encoder with white noise for some time. Decode the packets so that
  // when Opus generates DTX it does that based on the observed noise.
  uint32_t rtp_timestamp = 0xFFFu;
  for (int i = 0; i < 30; ++i) {
    generator.GenerateNextFrame(input_frame);
    rtc::Buffer payload;
    encoder.Encode(rtp_timestamp++, input_frame, &payload);
    if (payload.size() == 0) {
      continue;
    }
    std::vector<ParseResult> parse_results =
        decoder.ParsePayload(std::move(payload), /*timestamp=*/0);
    std::array<int16_t, kEncoderFrameLength * kDecoderNumChannels>
        decoded_frame;
    std::optional<DecodeResult> decode_results =
        parse_results[0].frame->Decode(decoded_frame);
    RTC_CHECK(decode_results);
    RTC_CHECK_EQ(decode_results->num_decoded_samples, decoded_frame.size());
  }

  // Decode an empty packet so that Opus treats it as DTX.
  rtc::Buffer payload;
  std::vector<ParseResult> parse_results =
      decoder.ParsePayload(std::move(payload), /*timestamp=*/0);
  RTC_CHECK_EQ(parse_results.size(), 1);
  ASSERT_TRUE(parse_results[0].frame->IsDtxPacket());
  // Decode the DTX packet. Comfort noise will be generated.
  const size_t num_samples =
      parse_results[0].frame->Duration() * kDecoderNumChannels;
  std::vector<int16_t> decoded_frame(num_samples);
  std::optional<DecodeResult> decode_results =
      parse_results[0].frame->Decode(decoded_frame);
  RTC_CHECK(decode_results);
  RTC_CHECK_EQ(decode_results->num_decoded_samples, decoded_frame.size());
  // Make sure that comfort noise is not a muted frame.
  ASSERT_FALSE(IsZeroedFrame(decoded_frame));

  // TODO: https://issues.webrtc.org/376493209 - When fixed, expect true below.
  EXPECT_FALSE(IsTrivialStereo(decoded_frame));
}

}  // namespace webrtc
