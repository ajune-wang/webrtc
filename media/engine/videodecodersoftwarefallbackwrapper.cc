/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/videodecodersoftwarefallbackwrapper.h"

#include <string>
#include <utility>

#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

VideoDecoderSoftwareFallbackWrapper::VideoDecoderSoftwareFallbackWrapper(
    std::unique_ptr<VideoDecoder> sw_fallback_decoder,
    std::unique_ptr<VideoDecoder> hw_decoder)
    : fallback_decoder_initialized_(false),
      hw_decoder_(std::move(hw_decoder)),
      hw_decoder_initialized_(false),
      fallback_decoder_(std::move(sw_fallback_decoder)),
      fallback_implementation_name_(
          std::string(fallback_decoder_->ImplementationName()) +
          " (fallback from: " + hw_decoder_->ImplementationName() + ")"),
      callback_(nullptr) {}

int32_t VideoDecoderSoftwareFallbackWrapper::InitDecode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores) {
  codec_settings_ = *codec_settings;
  number_of_cores_ = number_of_cores;

  int32_t status = InitHwDecoder();
  if (status == WEBRTC_VIDEO_CODEC_OK) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  if (InitFallbackDecoder()) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  return status;
}

int32_t VideoDecoderSoftwareFallbackWrapper::InitHwDecoder() {
  int32_t status = hw_decoder_->InitDecode(&codec_settings_, number_of_cores_);
  if (status != WEBRTC_VIDEO_CODEC_OK) {
    return status;
  }

  hw_decoder_initialized_ = true;
  // We should never initialize HW decoder while software fallback is active.
  RTC_DCHECK(!fallback_decoder_initialized_);

  if (callback_)
    hw_decoder_->RegisterDecodeCompleteCallback(callback_);
  return status;
}

bool VideoDecoderSoftwareFallbackWrapper::InitFallbackDecoder() {
  RTC_LOG(LS_WARNING) << "Decoder falling back to software decoding.";
  int32_t status =
      fallback_decoder_->InitDecode(&codec_settings_, number_of_cores_);
  if (status != WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_ERROR) << "Failed to initialize software-decoder fallback.";
    return false;
  }

  fallback_decoder_initialized_ = true;
  if (hw_decoder_initialized_) {
    hw_decoder_->Release();
    hw_decoder_initialized_ = false;
  }

  if (callback_)
    fallback_decoder_->RegisterDecodeCompleteCallback(callback_);
  return true;
}

int32_t VideoDecoderSoftwareFallbackWrapper::Decode(
    const EncodedImage& input_image,
    bool missing_frames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  TRACE_EVENT0("webrtc", "VideoDecoderSoftwareFallbackWrapper::Decode");
  if (!fallback_decoder_initialized_) {
    // Software fallback is not active or initializing it failed, try to use HW
    // decoding.
    int32_t ret = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    if (hw_decoder_initialized_ || InitHwDecoder() == WEBRTC_VIDEO_CODEC_OK) {
      ret = hw_decoder_->Decode(input_image, missing_frames, fragmentation,
                             codec_specific_info, render_time_ms);
    }
    if (ret != WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE) {
      return ret;
    }

    // HW decoder returned WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE or
    // initialization failed, fallback to software.
    if (!InitFallbackDecoder()) {
      return ret;
    }
  }
  return fallback_decoder_->Decode(input_image, missing_frames, fragmentation,
                                   codec_specific_info, render_time_ms);
}

int32_t VideoDecoderSoftwareFallbackWrapper::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  return fallback_decoder_initialized_
             ? fallback_decoder_->RegisterDecodeCompleteCallback(callback)
             : hw_decoder_->RegisterDecodeCompleteCallback(callback);
}

int32_t VideoDecoderSoftwareFallbackWrapper::Release() {
  if (fallback_decoder_initialized_) {
    RTC_LOG(LS_INFO) << "Releasing software fallback decoder.";
    // HW decoder should have already been released when software fallback was
    // activated.
    RTC_DCHECK(!hw_decoder_initialized_);
    fallback_decoder_initialized_ = false;
    return fallback_decoder_->Release();
  } else if (hw_decoder_initialized_) {
    hw_decoder_initialized_ = false;
    return hw_decoder_->Release();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

bool VideoDecoderSoftwareFallbackWrapper::PrefersLateDecoding() const {
  return fallback_decoder_initialized_
             ? fallback_decoder_->PrefersLateDecoding()
             : hw_decoder_->PrefersLateDecoding();
}

const char* VideoDecoderSoftwareFallbackWrapper::ImplementationName() const {
  return fallback_decoder_initialized_ ? fallback_implementation_name_.c_str()
                                       : hw_decoder_->ImplementationName();
}

}  // namespace webrtc
