/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP9_INCLUDE_VP9_H_
#define MODULES_VIDEO_CODING_CODECS_VP9_INCLUDE_VP9_H_

#include <memory>
#include <vector>

#include "api/transport/webrtc_key_value_config.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/codec.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/deprecation.h"

namespace webrtc {

// Returns a vector with all supported internal VP9 profiles that we can
// negotiate in SDP, in order of preference.
std::vector<SdpVideoFormat> SupportedVP9Codecs();

// Returns a vector with all supported internal VP9 decode profiles in order of
// preference. These will be availble for receive-only connections.
std::vector<SdpVideoFormat> SupportedVP9DecoderCodecs();

// Parses VP9 Profile from |codec|, configures experimental features of
// the encoder from |trials| and returns the approriate implementation.
std::unique_ptr<VideoEncoder> CreateVp9Encoder(
    const cricket::VideoCodec& codec);
std::unique_ptr<VideoEncoder> CreateVp9Encoder(
    const cricket::VideoCodec& codec,
    const WebRtcKeyValueConfig& trials);

namespace VP9Encoder {
// Returns default implementation using VP9 Profile 0.
// TODO(emircan): Remove once this is no longer used.
inline RTC_DEPRECATED std::unique_ptr<VideoEncoder> Create() {
  return CreateVp9Encoder({});
}
// Parses VP9 Profile from |codec| and returns the appropriate implementation.
inline RTC_DEPRECATED std::unique_ptr<VideoEncoder> Create(
    const cricket::VideoCodec& codec) {
  return CreateVp9Encoder(codec);
}
}  // namespace VP9Encoder

std::unique_ptr<VideoDecoder> CreateVp9Decoder();

namespace VP9Decoder {
inline RTC_DEPRECATED std::unique_ptr<VideoDecoder> Create() {
  return CreateVp9Decoder();
}
}  // namespace VP9Decoder
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP9_INCLUDE_VP9_H_
