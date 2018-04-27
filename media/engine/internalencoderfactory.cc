/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/internalencoderfactory.h"

#include <utility>

#include "api/video_codecs/sdp_video_format.h"
#if defined(USE_BUILTIN_SW_CODECS)
#include "media/engine/vp8_encoder_simulcast_proxy.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"  // nogncheck
#include "modules/video_coding/codecs/vp9/include/vp9.h"  // nogncheck
#endif
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {
namespace {
std::vector<SdpVideoFormat> SupportedFormats() {
  std::vector<SdpVideoFormat> supported_codecs;
#if defined(USE_BUILTIN_SW_CODECS)
  supported_codecs.push_back(SdpVideoFormat(cricket::kVp8CodecName));
  if (webrtc::VP9Encoder::IsSupported())
    supported_codecs.push_back(SdpVideoFormat(cricket::kVp9CodecName));

  for (const webrtc::SdpVideoFormat& format : webrtc::SupportedH264Codecs())
    supported_codecs.push_back(format);
#endif

  return supported_codecs;
}

VideoEncoderFactory::CodecInfo GetCodecInfo() {
  VideoEncoderFactory::CodecInfo info;
  info.is_hardware_accelerated = false;
  info.has_internal_source = false;
  return info;
}

class DefaultEncoderFactory : public VideoEncoderFactory {
 public:
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return SupportedFormats();
  }

  CodecInfo QueryVideoEncoder(const SdpVideoFormat& format) const override {
    return GetCodecInfo();
  }

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override {
#if defined(USE_BUILTIN_SW_CODECS)
  if (cricket::CodecNamesEq(format.name, cricket::kVp8CodecName))
    return VP8Encoder::Create();

  if (cricket::CodecNamesEq(format.name, cricket::kVp9CodecName))
    return VP9Encoder::Create();

  if (cricket::CodecNamesEq(format.name, cricket::kH264CodecName))
    return H264Encoder::Create(cricket::VideoCodec(format));
#endif

  RTC_LOG(LS_ERROR) << "Trying to created encoder of unsupported format "
                    << format.name;
  return nullptr;
  }
};

}  // namespace

InternalEncoderFactory::InternalEncoderFactory()
    : default_encoder_factory_(new DefaultEncoderFactory()) {}
InternalEncoderFactory::~InternalEncoderFactory() = default;

std::vector<SdpVideoFormat> InternalEncoderFactory::GetSupportedFormats()
    const {
  return SupportedFormats();
}

VideoEncoderFactory::CodecInfo InternalEncoderFactory::QueryVideoEncoder(
    const SdpVideoFormat& format) const {
  return GetCodecInfo();
}

std::unique_ptr<VideoEncoder> InternalEncoderFactory::CreateVideoEncoder(
    const SdpVideoFormat& format) {
#if defined(USE_BUILTIN_SW_CODECS)
  if (cricket::CodecNamesEq(format.name, cricket::kVp8CodecName)) {
    // Return wrapper that can fall back to simulcast encoder adapter if
    // simulcast settings aren't supported.
    return rtc::MakeUnique<VP8EncoderSimulcastProxy>(
        default_encoder_factory_.get());
  }
#endif

  return default_encoder_factory_->CreateVideoEncoder(format);
}

}  // namespace webrtc
