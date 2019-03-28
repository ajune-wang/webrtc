/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/opus/audio_decoder_multi_channel_opus_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "modules/audio_coding/codecs/opus/audio_coder_opus_common.h"
#include "rtc_base/string_to_number.h"

namespace webrtc {

AudioDecoderMultiChannelOpusImpl::AudioDecoderMultiChannelOpusImpl(
    AudioDecoderMultiChannelOpusConfig config)
    : config_(config) {
  RTC_DCHECK(config_.num_channels > 2);
  const int error = WebRtcOpus_MultistreamDecoderCreate(
      &dec_state_, config_.num_channels, config_.coupled_streams,
      config_.channel_mapping.data());
  RTC_DCHECK(error == 0);
  WebRtcOpus_DecoderInit(dec_state_);
}

AudioDecoderMultiChannelOpusImpl::~AudioDecoderMultiChannelOpusImpl() {
  WebRtcOpus_DecoderFree(dec_state_);
}

absl::optional<AudioDecoderMultiChannelOpusConfig>
AudioDecoderMultiChannelOpusImpl::SdpToConfig(const SdpAudioFormat& format) {
  if (format.num_channels <= 2) {
    return absl::nullopt;
  }

  AudioDecoderMultiChannelOpusConfig config;
  config.num_channels = format.num_channels;
  auto coupled_streams = GetFormatParameter<int>(format, "coupled_streams");
  if (!coupled_streams.has_value()) {
    return absl::nullopt;
  }
  config.coupled_streams = *coupled_streams;

  auto channel_mapping =
      GetFormatParameter<std::vector<int>>(format, "channel_mapping");
  if (!channel_mapping.has_value()) {
    return absl::nullopt;
  }
  // Convert to 'unsigned char':
  config.channel_mapping.clear();
  std::copy(channel_mapping->begin(), channel_mapping->end(),
            std::back_inserter(config.channel_mapping));

  RTC_DCHECK(config.IsOk());
  return config;
}

std::vector<AudioDecoder::ParseResult>
AudioDecoderMultiChannelOpusImpl::ParsePayload(rtc::Buffer&& payload,
                                               uint32_t timestamp) {
  std::vector<ParseResult> results;

  if (PacketHasFec(payload.data(), payload.size())) {
    const int duration =
        PacketDurationRedundant(payload.data(), payload.size());
    RTC_DCHECK_GE(duration, 0);
    rtc::Buffer payload_copy(payload.data(), payload.size());
    std::unique_ptr<EncodedAudioFrame> fec_frame(
        new OpusFrame(this, std::move(payload_copy), false));
    results.emplace_back(timestamp - duration, 1, std::move(fec_frame));
  }
  std::unique_ptr<EncodedAudioFrame> frame(
      new OpusFrame(this, std::move(payload), true));
  results.emplace_back(timestamp, 0, std::move(frame));
  return results;
}

int AudioDecoderMultiChannelOpusImpl::DecodeInternal(const uint8_t* encoded,
                                                     size_t encoded_len,
                                                     int sample_rate_hz,
                                                     int16_t* decoded,
                                                     SpeechType* speech_type) {
  RTC_DCHECK_EQ(sample_rate_hz, 48000);
  int16_t temp_type = 1;  // Default is speech.
  int ret =
      WebRtcOpus_Decode(dec_state_, encoded, encoded_len, decoded, &temp_type);
  if (ret > 0)
    ret *= static_cast<int>(
        config_.num_channels);  // Return total number of samples.
  *speech_type = ConvertSpeechType(temp_type);
  return ret;
}

int AudioDecoderMultiChannelOpusImpl::DecodeRedundantInternal(
    const uint8_t* encoded,
    size_t encoded_len,
    int sample_rate_hz,
    int16_t* decoded,
    SpeechType* speech_type) {
  if (!PacketHasFec(encoded, encoded_len)) {
    // This packet is a RED packet.
    return DecodeInternal(encoded, encoded_len, sample_rate_hz, decoded,
                          speech_type);
  }

  RTC_DCHECK_EQ(sample_rate_hz, 48000);
  int16_t temp_type = 1;  // Default is speech.
  int ret = WebRtcOpus_DecodeFec(dec_state_, encoded, encoded_len, decoded,
                                 &temp_type);
  if (ret > 0)
    ret *= static_cast<int>(
        config_.num_channels);  // Return total number of samples.
  *speech_type = ConvertSpeechType(temp_type);
  return ret;
}

void AudioDecoderMultiChannelOpusImpl::Reset() {
  WebRtcOpus_DecoderInit(dec_state_);
}

int AudioDecoderMultiChannelOpusImpl::PacketDuration(const uint8_t* encoded,
                                                     size_t encoded_len) const {
  return WebRtcOpus_DurationEst(dec_state_, encoded, encoded_len);
}

int AudioDecoderMultiChannelOpusImpl::PacketDurationRedundant(
    const uint8_t* encoded,
    size_t encoded_len) const {
  if (!PacketHasFec(encoded, encoded_len)) {
    // This packet is a RED packet.
    return PacketDuration(encoded, encoded_len);
  }

  return WebRtcOpus_FecDurationEst(encoded, encoded_len);
}

bool AudioDecoderMultiChannelOpusImpl::PacketHasFec(const uint8_t* encoded,
                                                    size_t encoded_len) const {
  int fec;
  fec = WebRtcOpus_PacketHasFec(encoded, encoded_len);
  return (fec == 1);
}

int AudioDecoderMultiChannelOpusImpl::SampleRateHz() const {
  return 48000;
}

size_t AudioDecoderMultiChannelOpusImpl::Channels() const {
  return config_.num_channels;
}

}  // namespace webrtc
