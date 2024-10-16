/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <iostream>
#include <optional>
#include <vector>

#include "api/array_view.h"
#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/opus/audio_encoder_opus_config.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "common_audio/wav_file.h"
#include "modules/audio_coding/codecs/opus/audio_decoder_opus.h"
#include "modules/audio_coding/codecs/opus/audio_encoder_opus.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"

namespace {

using webrtc::AudioDecoderOpusImpl;
using webrtc::AudioEncoderOpusConfig;
using webrtc::AudioEncoderOpusImpl;
using webrtc::Environment;
using webrtc::EnvironmentFactory;
using webrtc::WavReader;
using webrtc::WavWriter;

using DecodeResult = webrtc::AudioDecoder::EncodedAudioFrame::DecodeResult;
using ParseResult = webrtc::AudioDecoder::ParseResult;

constexpr int kSampleRateHz = 48000;

constexpr int kInputFrameDurationMs = 10;
constexpr int kInputFrameLength = kInputFrameDurationMs * kSampleRateHz / 1000;

constexpr int kEncoderFrameDurationMs = 20;
constexpr int kEncoderFrameLength =
    kEncoderFrameDurationMs * kSampleRateHz / 1000;

constexpr int kDecoderNumChannels = 2;

AudioEncoderOpusConfig GetEncoderConfig() {
  AudioEncoderOpusConfig config;

  config.frame_size_ms = kEncoderFrameDurationMs;
  config.sample_rate_hz = kSampleRateHz;
  config.num_channels = 1;
  config.application = AudioEncoderOpusConfig::ApplicationMode::kVoip;
  config.bitrate_bps = std::make_optional<int>(32000);
  config.fec_enabled = false;
  config.cbr_enabled = false;
  config.max_playback_rate_hz = kSampleRateHz;
  config.complexity = 10;
  config.dtx_enabled = false;

  return config;
}

constexpr float kPi = 3.14159265f;

class SineWaveGenerator {
 public:
  SineWaveGenerator(float wave_frequency_hz, int16_t amplitude)
      : phase_delta_(wave_frequency_hz * 2 * kPi / kSampleRateHz),
        amplitude_(amplitude) {
    RTC_DCHECK_GT(wave_frequency_hz, 0);
  }

  void GenerateNextFrame(std::array<int16_t, kInputFrameLength>& frame) {
    for (size_t i = 0; i < kInputFrameLength; ++i) {
      frame[i] = rtc::saturated_cast<int16_t>(amplitude_ * sinf(phase_));
      phase_ += phase_delta_;
    }
  }

 private:
  float phase_ = 0.0f;
  const float phase_delta_;
  const int16_t amplitude_;
};

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

int main(int argc, char* argv[]) {
  const Environment env = EnvironmentFactory().Create();
  const AudioEncoderOpusConfig encoder_config = GetEncoderConfig();
  AudioEncoderOpusImpl encoder(env, encoder_config, 111);
  AudioDecoderOpusImpl decoder(env.field_trials(), kDecoderNumChannels,
                               kSampleRateHz);

  std::array<int16_t, kInputFrameLength> input_frame;
  std::array<int16_t, kEncoderFrameLength * kDecoderNumChannels> decoded_frame;

  WavReader wav_reader("speech.wav");
  WavWriter wav_writer("opus_monoenc_stereodec.wav", kSampleRateHz,
                       kDecoderNumChannels);

  uint32_t rtp_timestamp = 0xFFFu;
  uint32_t timestamp = 0;

  int i = 0;
  while (++i) {
    if (wav_reader.ReadSamples(kInputFrameLength, input_frame.data()) !=
        kInputFrameLength) {
      break;
    }

    rtc::Buffer payload;
    encoder.Encode(rtp_timestamp++, input_frame, &payload);
    if (payload.size() == 0) {
      continue;
    }

    std::vector<ParseResult> parse_results =
        decoder.ParsePayload(std::move(payload), timestamp++);
    RTC_CHECK_EQ(parse_results.size(), 1);
    std::optional<DecodeResult> decode_results =
        parse_results[0].frame->Decode(decoded_frame);
    RTC_CHECK(decode_results);
    RTC_CHECK_EQ(decode_results->num_decoded_samples, decoded_frame.size());

    wav_writer.WriteSamples(decoded_frame.data(), decoded_frame.size());

    std::cout << "#" << i
              << " | is DTX: " << parse_results[0].frame->IsDtxPacket()
              << " | trivial stereo: "
              << (IsTrivialStereo(decoded_frame) ? "yes" : "NO") << std::endl;
  }

  return 0;
}
