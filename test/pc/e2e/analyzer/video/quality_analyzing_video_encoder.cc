/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/quality_analyzing_video_encoder.h"

#include <utility>

#include "absl/memory/memory.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

QualityAnalyzingVideoEncoder::QualityAnalyzingVideoEncoder(
    int id,
    std::unique_ptr<VideoEncoder> delegate,
    EncodedImageIdInjector* injector,
    VideoQualityAnalyzerInterface* analyzer)
    : id_(id),
      delegate_(std::move(delegate)),
      injector_(injector),
      analyzer_(analyzer) {
  analyzing_callback_ = absl::make_unique<EncoderCallback>(this);
}
QualityAnalyzingVideoEncoder::~QualityAnalyzingVideoEncoder() = default;

int32_t QualityAnalyzingVideoEncoder::InitEncode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores,
    size_t max_payload_size) {
  return delegate_->InitEncode(codec_settings, number_of_cores,
                               max_payload_size);
}

int32_t QualityAnalyzingVideoEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  rtc::CritScope crit(&lock_);
  delegate_callback_ = callback;
  return delegate_->RegisterEncodeCompleteCallback(analyzing_callback_.get());
}

int32_t QualityAnalyzingVideoEncoder::Release() {
  rtc::CritScope crit(&lock_);
  delegate_callback_ = nullptr;
  return delegate_->Release();
}

int32_t QualityAnalyzingVideoEncoder::Encode(
    const VideoFrame& frame,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  {
    rtc::CritScope crit(&lock_);
    // Store id to be able to retrieve it in analyzing callback.
    timestamp_to_frame_id_.insert({frame.timestamp(), frame.id()});
  }
  analyzer_->OnFramePreEncode(frame);
  int32_t result = delegate_->Encode(frame, codec_specific_info, frame_types);
  if (result != WEBRTC_VIDEO_CODEC_OK) {
    // If origin encoder failed, then cleanup data for this frame.
    {
      rtc::CritScope crit(&lock_);
      timestamp_to_frame_id_.erase(frame.timestamp());
    }
    analyzer_->OnEncoderError(frame, result);
  }
  return result;
}

int32_t QualityAnalyzingVideoEncoder::SetRates(uint32_t bitrate,
                                               uint32_t framerate) {
  return delegate_->SetRates(bitrate, framerate);
}

int32_t QualityAnalyzingVideoEncoder::SetRateAllocation(
    const VideoBitrateAllocation& allocation,
    uint32_t framerate) {
  return delegate_->SetRateAllocation(allocation, framerate);
}

VideoEncoder::EncoderInfo QualityAnalyzingVideoEncoder::GetEncoderInfo() const {
  return delegate_->GetEncoderInfo();
}

QualityAnalyzingVideoEncoder::EncoderCallback::EncoderCallback(
    QualityAnalyzingVideoEncoder* encoder)
    : encoder_(encoder) {}
QualityAnalyzingVideoEncoder::EncoderCallback::~EncoderCallback() = default;

EncodedImageCallback::Result
QualityAnalyzingVideoEncoder::EncoderCallback::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  uint16_t frame_id;
  {
    rtc::CritScope crit(&encoder_->lock_);
    auto it = encoder_->timestamp_to_frame_id_.find(encoded_image.Timestamp());
    if (it == encoder_->timestamp_to_frame_id_.end()) {
      // Ensure, that we have info about this frame.
      RTC_LOG(WARNING) << "No frame id for frame with timestamp: "
                       << encoded_image.Timestamp();
      return EncodedImageCallback::Result(
          EncodedImageCallback::Result::Error::OK);
    }
    frame_id = it->second;
    encoder_->timestamp_to_frame_id_.erase(it);
  }

  encoder_->analyzer_->OnFrameEncoded(frame_id, encoded_image);

  const EncodedImage& image =
      encoder_->injector_->InjectId(frame_id, encoded_image, encoder_->id_);
  {
    rtc::CritScope crit(&encoder_->lock_);
    if (!encoder_->delegate_callback_) {
      return EncodedImageCallback::Result(
          EncodedImageCallback::Result::Error::OK);
    }
    return encoder_->delegate_callback_->OnEncodedImage(
        image, codec_specific_info, fragmentation);
  }
}

void QualityAnalyzingVideoEncoder::EncoderCallback::OnDroppedFrame(
    EncodedImageCallback::DropReason reason) {
  rtc::CritScope crit(&encoder_->lock_);
  encoder_->analyzer_->OnFrameDropped(reason);
  encoder_->delegate_callback_->OnDroppedFrame(reason);
}

QualityAnalyzingVideoEncoderFactory::QualityAnalyzingVideoEncoderFactory(
    std::unique_ptr<VideoEncoderFactory> delegate,
    IdGenerator<int>* id_generator,
    EncodedImageIdInjector* injector,
    VideoQualityAnalyzerInterface* analyzer)
    : delegate_(std::move(delegate)),
      id_generator_(id_generator),
      injector_(injector),
      analyzer_(analyzer) {}
QualityAnalyzingVideoEncoderFactory::~QualityAnalyzingVideoEncoderFactory() =
    default;

std::vector<SdpVideoFormat>
QualityAnalyzingVideoEncoderFactory::GetSupportedFormats() const {
  return delegate_->GetSupportedFormats();
}

VideoEncoderFactory::CodecInfo
QualityAnalyzingVideoEncoderFactory::QueryVideoEncoder(
    const SdpVideoFormat& format) const {
  return delegate_->QueryVideoEncoder(format);
}

std::unique_ptr<VideoEncoder>
QualityAnalyzingVideoEncoderFactory::CreateVideoEncoder(
    const SdpVideoFormat& format) {
  return absl::make_unique<QualityAnalyzingVideoEncoder>(
      id_generator_->GetNextId(), delegate_->CreateVideoEncoder(format),
      injector_, analyzer_);
}

}  // namespace test
}  // namespace webrtc
