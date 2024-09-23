/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/black_frame_decoder_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "api/environment/environment.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "modules/video_coding/black_frame_decoder.h"
#include "rtc_base/logging.h"

namespace webrtc {

BlackFrameDecoderFactory::BlackFrameDecoderFactory(
    std::unique_ptr<VideoDecoderFactory> factory)
    : factory_(std::move(factory)) {}

std::vector<SdpVideoFormat> BlackFrameDecoderFactory::GetSupportedFormats()
    const {
  return factory_->GetSupportedFormats();
}

VideoDecoderFactory::CodecSupport BlackFrameDecoderFactory::QueryCodecSupport(
    const SdpVideoFormat& format,
    bool reference_scaling) const {
  return factory_->QueryCodecSupport(format, reference_scaling);
}

std::unique_ptr<VideoDecoder> BlackFrameDecoderFactory::Create(
    const Environment& env,
    const SdpVideoFormat& format) {
  if (!format.IsCodecInList(GetSupportedFormats())) {
    RTC_LOG(LS_WARNING) << "Trying to create decoder for unsupported format. "
                        << format.ToString();
    return nullptr;
  }
  return std::make_unique<BlackFrameDecoder>(format);
}

}  // namespace webrtc
