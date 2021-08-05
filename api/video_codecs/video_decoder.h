/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_DECODER_H_
#define API_VIDEO_CODECS_VIDEO_DECODER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/video/encoded_image.h"
#include "api/video/render_resolution.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT DecodedImageCallback {
 public:
  virtual ~DecodedImageCallback() {}

  virtual int32_t Decoded(VideoFrame& decodedImage) = 0;
  // Provides an alternative interface that allows the decoder to specify the
  // decode time excluding waiting time for any previous pending frame to
  // return. This is necessary for breaking positive feedback in the delay
  // estimation when the decoder has a single output buffer.
  virtual int32_t Decoded(VideoFrame& decodedImage, int64_t decode_time_ms);

  // TODO(sakal): Remove other implementations when upstream projects have been
  // updated.
  virtual void Decoded(VideoFrame& decodedImage,
                       absl::optional<int32_t> decode_time_ms,
                       absl::optional<uint8_t> qp);
};

class RTC_EXPORT VideoDecoder {
 public:
  class Config {
   public:
    Config() = default;
    Config(const Config&) = default;
    Config& operator=(const Config&) = default;
    ~Config() = default;

    // The size of pool which is used to store video frame buffers inside
    // decoder. If value isn't present some codec-default value will be used. If
    // value is present and decoder doesn't have buffer pool the value will be
    // ignored.
    absl::optional<int> buffer_pool_size() const { return buffer_pool_size_; }
    void set_buffer_pool_size(absl::optional<int> value) {
      buffer_pool_size_ = value;
    }

    RenderResolution max_encoded_resolution() const { return resolution_; }
    void set_max_encoded_resolution(RenderResolution value) {
      resolution_ = value;
    }

    int number_of_cores() const { return number_of_cores_; }
    void set_number_of_cores(int value) { number_of_cores_ = value; }

    VideoCodecType codec() const { return codec_; }
    void set_codec(VideoCodecType value) { codec_ = value; }

   private:
    absl::optional<int> buffer_pool_size_;
    RenderResolution resolution_;
    int number_of_cores_ = 1;
    VideoCodecType codec_ = kVideoCodecGeneric;
  };

  struct DecoderInfo {
    // Descriptive name of the decoder implementation.
    std::string implementation_name;

    // True if the decoder is backed by hardware acceleration.
    bool is_hardware_accelerated = false;

    std::string ToString() const;
    bool operator==(const DecoderInfo& rhs) const;
    bool operator!=(const DecoderInfo& rhs) const { return !(*this == rhs); }
  };

  virtual ~VideoDecoder() {}

  ABSL_DEPRECATED("Use Init instead")
  virtual int32_t InitDecode(const VideoCodec* codec_settings,
                             int32_t number_of_cores);

  // Configures the decoder. Returns false on failure.
  virtual bool Init(const Config& config) = 0;

  virtual int32_t Decode(const EncodedImage& input_image,
                         bool missing_frames,
                         int64_t render_time_ms) = 0;

  virtual int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) = 0;

  virtual int32_t Release() = 0;

  virtual DecoderInfo GetDecoderInfo() const;

  // Deprecated, use GetDecoderInfo().implementation_name instead.
  virtual const char* ImplementationName() const;

  static Config LegacyConfig(const VideoCodec* codec_settings,
                             int32_t number_of_cores);
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_DECODER_H_
