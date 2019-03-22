/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_MULTI_CHANNEL_OPUS_CONFIG_H_
#define API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_MULTI_CHANNEL_OPUS_CONFIG_H_

#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/opus/audio_encoder_opus_config.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT AudioEncoderMultiChannelOpusConfig {
  AudioEncoderOpusConfig single_stream_config;

  bool IsOk() const {
    // Check the channel mapping. The LENGTH has to be checked BEFORE passing a
    // raw pointer to *_opus_create(). The content doesn't HAVE to be checked,
    // but maybe SHOULD be checked anyway.

    // Also, what if the channel mapping is larger than necessary? Somewhere in
    // the RFC, it maybe says that it has to be 255 bytes (but only the first
    // few are used).
    return single_stream_config.IsOk() &&
           channel_mapping.size() >= single_stream_config.num_channels &&
           channel_mapping.size() <= 255;
  }

  // RFC 7845 stuff: TODO(aleloi): describe.
  int coupled_streams;
  std::vector<unsigned char> channel_mapping;
};

}  // namespace webrtc
#endif  // API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_MULTI_CHANNEL_OPUS_CONFIG_H_
