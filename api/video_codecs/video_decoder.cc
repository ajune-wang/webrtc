/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_decoder.h"

#include "rtc_base/strings/string_builder.h"

namespace webrtc {

int32_t DecodedImageCallback::Decoded(VideoFrame& decodedImage,
                                      int64_t decode_time_ms) {
  // The default implementation ignores custom decode time value.
  return Decoded(decodedImage);
}

void DecodedImageCallback::Decoded(VideoFrame& decodedImage,
                                   absl::optional<int32_t> decode_time_ms,
                                   absl::optional<uint8_t> qp) {
  Decoded(decodedImage, decode_time_ms.value_or(-1));
}

int32_t VideoDecoder::InitDecode(const VideoCodec* codec_settings,
                                 int32_t number_of_cores) {
  return Init(LegacyConfig(codec_settings, number_of_cores)) ? 0 : -1;
}

VideoDecoder::DecoderInfo VideoDecoder::GetDecoderInfo() const {
  DecoderInfo info;
  info.implementation_name = ImplementationName();
  return info;
}

const char* VideoDecoder::ImplementationName() const {
  return "unknown";
}

std::string VideoDecoder::DecoderInfo::ToString() const {
  char string_buf[2048];
  rtc::SimpleStringBuilder oss(string_buf);

  oss << "DecoderInfo { "
      << "prefers_late_decoding = "
      << "implementation_name = '" << implementation_name << "', "
      << "is_hardware_accelerated = "
      << (is_hardware_accelerated ? "true" : "false") << " }";
  return oss.str();
}

bool VideoDecoder::DecoderInfo::operator==(const DecoderInfo& rhs) const {
  return is_hardware_accelerated == rhs.is_hardware_accelerated &&
         implementation_name == rhs.implementation_name;
}

// static
VideoDecoder::Config VideoDecoder::LegacyConfig(
    const VideoCodec* codec_settings,
    int32_t number_of_cores) {
  VideoDecoder::Config config;
  if (codec_settings != nullptr) {
    config.set_buffer_pool_size(codec_settings->buffer_pool_size);
    config.set_max_encoded_resolution(
        {codec_settings->width, codec_settings->height});
    config.set_codec(codec_settings->codecType);
  }
  config.set_number_of_cores(number_of_cores);
  return config;
}

}  // namespace webrtc
