/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_VP8_ENCODER_FACTORY_H_
#define TEST_VP8_ENCODER_FACTORY_H_

#include <memory>
#include <vector>

#include "api/video_codecs/video_encoder_factory.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {
namespace test {

// An encoder factory producing VP8 encoders only.
class Vp8EncoderFactory : public VideoEncoderFactory {
 public:
  Vp8EncoderFactory() = default;

  // Unused by tests.
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    RTC_NOTREACHED();
  }

  CodecInfo QueryVideoEncoder(const SdpVideoFormat& format) const override {
    CodecInfo codec_info;
    codec_info.is_hardware_accelerated = false;
    codec_info.has_internal_source = false;
    return codec_info;
  }

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override {
    return VP8Encoder::Create();
  }
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_VP8_ENCODER_FACTORY_H_
