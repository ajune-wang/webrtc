/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_OPUS_AUDIO_DECODER_MULTI_CHANNEL_OPUS_CONFIG_H_
#define API_AUDIO_CODECS_OPUS_AUDIO_DECODER_MULTI_CHANNEL_OPUS_CONFIG_H_

#include <vector>

namespace webrtc {
struct AudioDecoderMultiChannelOpusConfig {
  int num_channels;

  // Number of channel pairs coupled together, see RFC 7845 section
  // 5.1.1.
  int coupled_streams;

  // Channel mapping table, defines the mapping from encoded streams to output
  // channels. See RFC 7845 section 5.1.1.
  std::vector<unsigned char> channel_mapping;

  bool IsOk() const {
    return channel_mapping.size() >= static_cast<size_t>(num_channels) &&
           channel_mapping.size() <= 255;
  }
};

}  // namespace webrtc

#endif  //  API_AUDIO_CODECS_OPUS_AUDIO_DECODER_MULTI_CHANNEL_OPUS_CONFIG_H_
