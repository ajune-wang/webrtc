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

#include "absl/strings/match.h"
#include "modules/audio_coding/codecs/opus/audio_encoder_multi_channel_opus.h"

namespace webrtc {

absl::optional<AudioEncoderOpusConfig>
AudioEncoderMultiChannelOpus::SdpToConfig(const SdpAudioFormat& format) {
  // AudioEncoderOpusImpl supports both 'opus' and 'multiopus'. But in this
  // codec layer, we only allow 'multiopus'.
  if (!absl::EqualsIgnoreCase(format.name, "multiopus")) {
    return absl::nullopt;
  }
  return AudioEncoderMultiChannelOpusImpl::SdpToConfig(format);
}

void AudioEncoderMultiChannelOpus::AppendSupportedEncoders(
    std::vector<AudioCodecSpec>* specs) {
  AudioEncoderMultiChannelOpusImpl::AppendSupportedEncoders(specs);
}

AudioCodecInfo AudioEncoderMultiChannelOpus::QueryAudioEncoder(
    const AudioEncoderOpusConfig& config) {
  return AudioEncoderMultiChannelOpusImpl::QueryAudioEncoder(config);
}

std::unique_ptr<AudioEncoder> AudioEncoderMultiChannelOpus::MakeAudioEncoder(
    const AudioEncoderOpusConfig& config,
    int payload_type,
    absl::optional<AudioCodecPairId> /*codec_pair_id*/) {
  return AudioEncoderMultiChannelOpusImpl::MakeAudioEncoder(config,
                                                            payload_type);
}

}  // namespace webrtc
