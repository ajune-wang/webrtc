/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/opus/audio_decoder_multi_channel_opus.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "modules/audio_coding/codecs/opus/audio_decoder_multi_channel_opus_impl.h"

namespace webrtc {

absl::optional<AudioDecoderMultiChannelOpusConfig>
AudioDecoderMultiChannelOpus::SdpToConfig(const SdpAudioFormat& format) {
  return AudioDecoderMultiChannelOpusImpl::SdpToConfig(format);
  // // TODO(aleloi): check this...
  // if (absl::EqualsIgnoreCase(format.name, "multiopus") &&
  //     format.clockrate_hz == 48000 &&
  //     (format.num_channels == 4 || format.num_channels == 6 ||
  //      format.num_channels == 8) &&
  //     format.num_channels) {
  //   return Config{static_cast<int>(format.num_channels)};
  // } else {
  //   return absl::nullopt;
  // }
}

void AudioDecoderMultiChannelOpus::AppendSupportedDecoders(
    std::vector<AudioCodecSpec>* specs) {
  // Or? How was it this stuff worked? Should it be announced as supported if we
  // want to use it?

  // AudioCodecInfo opus_info{48000, 1, 64000, 6000, 510000};
  // opus_info.allow_comfort_noise = false;
  // opus_info.supports_network_adaption = true;
  // SdpAudioFormat opus_format(
  //     {"multiopus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}});
  // specs->push_back({std::move(opus_format), opus_info});
}

std::unique_ptr<AudioDecoder> AudioDecoderMultiChannelOpus::MakeAudioDecoder(
    AudioDecoderMultiChannelOpusConfig config,
    absl::optional<AudioCodecPairId> /*codec_pair_id*/) {
  return absl::make_unique<AudioDecoderMultiChannelOpusImpl>(config);
}
}  // namespace webrtc
