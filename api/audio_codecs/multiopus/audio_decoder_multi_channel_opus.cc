/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/multiopus/audio_decoder_multi_channel_opus.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "modules/audio_coding/codecs/opus/audio_decoder_opus.h"

namespace webrtc {

absl::optional<AudioDecoderMultiChannelOpus::Config>
AudioDecoderMultiChannelOpus::SdpToConfig(const SdpAudioFormat& format) {
  if (absl::EqualsIgnoreCase(format.name, "multiopus") &&
      format.clockrate_hz == 48000 &&
      (format.num_channels == 4 || format.num_channels == 6 ||
       format.num_channels == 8) &&
      format.num_channels) {
    return Config{static_cast<int>(format.num_channels)};
  } else {
    return absl::nullopt;
  }
}

void AudioDecoderMultiChannelOpus::AppendSupportedDecoders(
    std::vector<AudioCodecSpec>* specs) {
  AudioCodecInfo opus_info{48000, 1, 64000, 6000, 510000};
  opus_info.allow_comfort_noise = false;
  opus_info.supports_network_adaption = true;
  SdpAudioFormat opus_format(
      {"multiopus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}});
  specs->push_back({std::move(opus_format), opus_info});
}

std::unique_ptr<AudioDecoder> AudioDecoderMultiChannelOpus::MakeAudioDecoder(
    Config config,
    absl::optional<AudioCodecPairId> /*codec_pair_id*/) {
  return absl::make_unique<AudioDecoderOpusImpl>(config.num_channels);
}

}  // namespace webrtc
