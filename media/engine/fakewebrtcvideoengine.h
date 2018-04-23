/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_FAKEWEBRTCVIDEOENGINE_H_
#define MEDIA_ENGINE_FAKEWEBRTCVIDEOENGINE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/base/codec.h"
#include "media/engine/internaldecoderfactory.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "media/engine/vp8_encoder_simulcast_proxy.h"
#include "media/engine/webrtcvideodecoderfactory.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/basictypes.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/thread_annotations.h"

namespace cricket {

static const int kEventTimeoutMs = 10000;

class FakeWebRtcVideoDecoderFactory;
class FakeWebRtcVideoEncoderFactory;

bool IsFormatSupported2(
    const std::vector<webrtc::SdpVideoFormat>& supported_formats,
    const webrtc::SdpVideoFormat& format);

// Fake class for mocking out webrtc::VideoDecoder
class FakeWebRtcVideoDecoder : public webrtc::VideoDecoder {
 public:
  explicit FakeWebRtcVideoDecoder(FakeWebRtcVideoDecoderFactory* factory);
  ~FakeWebRtcVideoDecoder();

  virtual int32_t InitDecode(const webrtc::VideoCodec*, int32_t) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32_t Decode(const webrtc::EncodedImage&,
                         bool,
                         const webrtc::RTPFragmentationHeader*,
                         const webrtc::CodecSpecificInfo*,
                         int64_t) {
    num_frames_received_++;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback*) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32_t Release() { return WEBRTC_VIDEO_CODEC_OK; }

  int GetNumFramesReceived() const {
    return num_frames_received_;
  }

 private:
  int num_frames_received_;
  FakeWebRtcVideoDecoderFactory* factory_;
};

// Fake class for mocking out webrtc::VideoDecoderFactory.
class FakeWebRtcVideoDecoderFactory : public webrtc::VideoDecoderFactory {
 public:
  FakeWebRtcVideoDecoderFactory()
      : num_created_decoders_(0),
        internal_decoder_factory_(new webrtc::InternalDecoderFactory()) {}

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<webrtc::SdpVideoFormat> formats =
        internal_decoder_factory_->GetSupportedFormats();

    // Add external codecs.
    for (const webrtc::SdpVideoFormat& format : supported_codec_formats_) {
      // Don't add same codec twice.
      if (!IsFormatSupported2(formats, format))
        formats.push_back(format);
    }

    return formats;
  }

  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) override {
    if (std::find(supported_codec_formats_.begin(),
                  supported_codec_formats_.end(),
                  format) != supported_codec_formats_.end()) {
      num_created_decoders_++;
      std::unique_ptr<FakeWebRtcVideoDecoder> decoder =
          rtc::MakeUnique<FakeWebRtcVideoDecoder>(this);
      decoders_.push_back(decoder.get());
      return decoder;
    } else {
      return internal_decoder_factory_->CreateVideoDecoder(format);
    }
  }

  void DecoderDestroyed(FakeWebRtcVideoDecoder* decoder) {
    decoders_.erase(std::remove(decoders_.begin(), decoders_.end(), decoder),
                    decoders_.end());
  }

  void AddSupportedVideoCodecType(const webrtc::SdpVideoFormat& format) {
    supported_codec_formats_.push_back(format);
  }

  int GetNumCreatedDecoders() {
    return num_created_decoders_;
  }

  const std::vector<FakeWebRtcVideoDecoder*>& decoders() {
    return decoders_;
  }

 private:
  std::vector<webrtc::SdpVideoFormat> supported_codec_formats_;
  std::vector<FakeWebRtcVideoDecoder*> decoders_;
  int num_created_decoders_;
  webrtc::InternalDecoderFactory* internal_decoder_factory_;
};

// Fake class for mocking out webrtc::VideoEnoder
class FakeWebRtcVideoEncoder : public webrtc::VideoEncoder {
 public:
  explicit FakeWebRtcVideoEncoder(FakeWebRtcVideoEncoderFactory* factory);
  ~FakeWebRtcVideoEncoder();

  int32_t InitEncode(const webrtc::VideoCodec* codecSettings,
                     int32_t numberOfCores,
                     size_t maxPayloadSize) override {
    rtc::CritScope lock(&crit_);
    codec_settings_ = *codecSettings;
    init_encode_event_.Set();
    return WEBRTC_VIDEO_CODEC_OK;
  }

  bool WaitForInitEncode() { return init_encode_event_.Wait(kEventTimeoutMs); }

  webrtc::VideoCodec GetCodecSettings() {
    rtc::CritScope lock(&crit_);
    return codec_settings_;
  }

  int32_t Encode(const webrtc::VideoFrame& inputImage,
                 const webrtc::CodecSpecificInfo* codecSpecificInfo,
                 const std::vector<webrtc::FrameType>* frame_types) override {
    rtc::CritScope lock(&crit_);
    ++num_frames_encoded_;
    init_encode_event_.Set();
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }

  int32_t SetChannelParameters(uint32_t packetLoss, int64_t rtt) override {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t SetRateAllocation(const webrtc::BitrateAllocation& allocation,
                            uint32_t framerate) override {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int GetNumEncodedFrames() {
    rtc::CritScope lock(&crit_);
    return num_frames_encoded_;
  }

 private:
  rtc::CriticalSection crit_;
  rtc::Event init_encode_event_;
  int num_frames_encoded_ RTC_GUARDED_BY(crit_);
  webrtc::VideoCodec codec_settings_ RTC_GUARDED_BY(crit_);
  FakeWebRtcVideoEncoderFactory* factory_;
};

// Fake class for mocking out webrtc::VideoEncoderFactory.
class FakeWebRtcVideoEncoderFactory : public webrtc::VideoEncoderFactory {
 public:
  FakeWebRtcVideoEncoderFactory()
      : created_video_encoder_event_(false, false),
        num_created_encoders_(0),
        encoders_have_internal_sources_(false),
        internal_encoder_factory_(new webrtc::InternalEncoderFactory()),
        vp8_factory_mode_(false) {}

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<webrtc::SdpVideoFormat> formats =
        internal_encoder_factory_->GetSupportedFormats();

    // Add external codecs.
    for (const webrtc::SdpVideoFormat& format : formats_) {
      // Don't add same codec twice.
      if (!IsFormatSupported2(formats, format))
        formats.push_back(format);
    }

    return formats;
  }

  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override {
    rtc::CritScope lock(&crit_);
    std::unique_ptr<webrtc::VideoEncoder> encoder;
    if (std::find(formats_.begin(), formats_.end(), format) != formats_.end()) {
      if (CodecNamesEq(format.name.c_str(), kVp8CodecName) &&
          !vp8_factory_mode_) {
        // The simulcast adapter will ask this factory for multiple VP8
        // encoders. Enter vp8_factory_mode so that we now create these encoders
        // instead of more adapters.
        vp8_factory_mode_ = true;
        encoder = rtc::MakeUnique<webrtc::SimulcastEncoderAdapter>(this);
      } else {
        num_created_encoders_++;
        created_video_encoder_event_.Set();
        encoder = rtc::MakeUnique<FakeWebRtcVideoEncoder>(this);
        encoders_.push_back(
            static_cast<FakeWebRtcVideoEncoder*>(encoder.get()));
      }
    } else {
      encoder = CodecNamesEq(format.name.c_str(), kVp8CodecName)
                    ? rtc::MakeUnique<webrtc::VP8EncoderSimulcastProxy>(
                          internal_encoder_factory_)
                    : internal_encoder_factory_->CreateVideoEncoder(format);
    }
    return encoder;
  }

  CodecInfo QueryVideoEncoder(
      const webrtc::SdpVideoFormat& format) const override {
    if (std::find(formats_.begin(), formats_.end(), format) != formats_.end()) {
      webrtc::VideoEncoderFactory::CodecInfo info;
      info.has_internal_source = encoders_have_internal_sources_;
      info.is_hardware_accelerated = true;
      return info;
    } else {
      return internal_encoder_factory_->QueryVideoEncoder(format);
    }
  };

  bool WaitForCreatedVideoEncoders(int num_encoders) {
    int64_t start_offset_ms = rtc::TimeMillis();
    int64_t wait_time = kEventTimeoutMs;
    do {
      if (GetNumCreatedEncoders() >= num_encoders)
        return true;
      wait_time = kEventTimeoutMs - (rtc::TimeMillis() - start_offset_ms);
    } while (wait_time > 0 && created_video_encoder_event_.Wait(wait_time));
    return false;
  }

  void EncoderDestroyed(FakeWebRtcVideoEncoder* encoder) {
    rtc::CritScope lock(&crit_);
    encoders_.erase(std::remove(encoders_.begin(), encoders_.end(), encoder),
                    encoders_.end());
  }

  void set_encoders_have_internal_sources(bool internal_source) {
    encoders_have_internal_sources_ = internal_source;
  }

  void AddSupportedVideoCodec(const webrtc::SdpVideoFormat& format) {
    formats_.push_back(format);
  }

  void AddSupportedVideoCodecType(const std::string& name) {
    formats_.push_back(webrtc::SdpVideoFormat(name));
  }

  int GetNumCreatedEncoders() {
    rtc::CritScope lock(&crit_);
    return num_created_encoders_;
  }

  const std::vector<FakeWebRtcVideoEncoder*> encoders() {
    rtc::CritScope lock(&crit_);
    return encoders_;
  }

 private:
  rtc::CriticalSection crit_;
  rtc::Event created_video_encoder_event_;
  std::vector<webrtc::SdpVideoFormat> formats_;
  std::vector<FakeWebRtcVideoEncoder*> encoders_ RTC_GUARDED_BY(crit_);
  int num_created_encoders_ RTC_GUARDED_BY(crit_);
  bool encoders_have_internal_sources_;
  webrtc::InternalEncoderFactory* internal_encoder_factory_;
  bool vp8_factory_mode_;
};

}  // namespace cricket

#endif  // MEDIA_ENGINE_FAKEWEBRTCVIDEOENGINE_H_
