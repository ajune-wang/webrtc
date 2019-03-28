/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/opus/audio_decoder_multi_channel_opus_config.h"
#include "modules/audio_coding/codecs/opus/audio_decoder_multi_channel_opus_impl.h"
#include "test/fuzzers/audio_decoder_fuzzer.h"

namespace webrtc {

AudioDecoderMultiChannelOpusConfig MakeDecoderConfig(
    int num_channels,
    int coupled_streams,
    std::vector<unsigned char> channel_mapping) {
  AudioDecoderMultiChannelOpusConfig config;
  config.num_channels = num_channels;
  config.coupled_streams = coupled_streams;
  config.channel_mapping = channel_mapping;
  return config;
}

void FuzzOneInput(const uint8_t* data, size_t size) {
  const std::vector<AudioDecoderMultiChannelOpusConfig> surround_configs = {
      MakeDecoderConfig(2, 2, {0, 1, 2, 3}),             // Quad.
      MakeDecoderConfig(6, 2, {0, 4, 1, 2, 3, 5}),       // 5.1
      MakeDecoderConfig(8, 3, {0, 6, 1, 2, 3, 5, 5, 7})  // 7.1
  };

  const auto config = surround_configs[data[0] % 3];  // 1 or 2 channels.
  AudioDecoderMultiChannelOpusImpl dec(config);
  const int kSampleRateHz = 48000;
  const size_t kAllocatedOuputSizeSamples =
      4 * kSampleRateHz / 10;  // 4x100 ms.
  int16_t output[kAllocatedOuputSizeSamples];
  FuzzAudioDecoder(DecoderFunctionType::kNormalDecode, data, size, &dec,
                   kSampleRateHz, sizeof(output), output);
}
}  // namespace webrtc
