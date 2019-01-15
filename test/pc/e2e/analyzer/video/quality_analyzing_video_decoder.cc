/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/quality_analyzing_video_decoder.h"

#include <cstdint>
#include <cstring>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

QualityAnalyzingVideoDecoder::QualityAnalyzingVideoDecoder(
    int id,
    std::unique_ptr<VideoDecoder> delegate,
    EncodedImageIdExtractor* extractor,
    VideoQualityAnalyzerInterface* analyzer)
    : id_(id),
      delegate_(std::move(delegate)),
      extractor_(extractor),
      analyzer_(analyzer),
      delegate_callback_(nullptr) {
  analyzing_callback_ = absl::make_unique<DecoderCallback>(this);
}
QualityAnalyzingVideoDecoder::~QualityAnalyzingVideoDecoder() = default;

int32_t QualityAnalyzingVideoDecoder::InitDecode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores) {
  return delegate_->InitDecode(codec_settings, number_of_cores);
}

int32_t QualityAnalyzingVideoDecoder::Decode(
    const EncodedImage& input_image,
    bool missing_frames,
    const CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  std::pair<uint16_t, EncodedImage> out =
      extractor_->ExtractId(input_image, id_);

  EncodedImage* origin_image;
  {
    rtc::CritScope crit(&lock_);
    // Store id to be able to retrieve it in analyzing callback.
    timestamp_to_frame_id_.insert({input_image.Timestamp(), out.first});
    // Store encoded image to prevent its destruction while it is used in
    // decoder.
    origin_image = &decoding_images_.insert({out.first, std::move(out.second)})
                        .first->second;
  }
  analyzer_->OnFrameReceived(out.first, *origin_image);
  int32_t result = delegate_->Decode(*origin_image, missing_frames,
                                     codec_specific_info, render_time_ms);
  if (result != WEBRTC_VIDEO_CODEC_OK) {
    // If origin decoder failed, then cleanup data for this image.
    {
      rtc::CritScope crit(&lock_);
      timestamp_to_frame_id_.erase(input_image.Timestamp());
      decoding_images_.erase(out.first);
    }
    analyzer_->OnDecoderError(out.first, result);
  }
  return result;
}

int32_t QualityAnalyzingVideoDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  rtc::CritScope crit(&lock_);
  delegate_callback_ = callback;
  return delegate_->RegisterDecodeCompleteCallback(analyzing_callback_.get());
}

int32_t QualityAnalyzingVideoDecoder::Release() {
  rtc::CritScope crit(&lock_);
  delegate_callback_ = nullptr;
  return delegate_->Release();
}

bool QualityAnalyzingVideoDecoder::PrefersLateDecoding() const {
  return delegate_->PrefersLateDecoding();
}

const char* QualityAnalyzingVideoDecoder::ImplementationName() const {
  return delegate_->ImplementationName();
}

QualityAnalyzingVideoDecoder::DecoderCallback::DecoderCallback(
    QualityAnalyzingVideoDecoder* decoder)
    : decoder_(decoder) {}
QualityAnalyzingVideoDecoder::DecoderCallback::~DecoderCallback() = default;

int32_t QualityAnalyzingVideoDecoder::DecoderCallback::Decoded(
    VideoFrame& decodedImage) {
  OnFrameDecoded(&decodedImage, absl::nullopt, absl::nullopt);

  rtc::CritScope crit(&decoder_->lock_);
  if (!decoder_->delegate_callback_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  return decoder_->delegate_callback_->Decoded(decodedImage);
}

int32_t QualityAnalyzingVideoDecoder::DecoderCallback::Decoded(
    VideoFrame& decodedImage,
    int64_t decode_time_ms) {
  OnFrameDecoded(&decodedImage, decode_time_ms, absl::nullopt);

  rtc::CritScope crit(&decoder_->lock_);
  if (!decoder_->delegate_callback_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  return decoder_->delegate_callback_->Decoded(decodedImage, decode_time_ms);
}

void QualityAnalyzingVideoDecoder::DecoderCallback::Decoded(
    VideoFrame& decodedImage,
    absl::optional<int32_t> decode_time_ms,
    absl::optional<uint8_t> qp) {
  OnFrameDecoded(&decodedImage, decode_time_ms, qp);

  rtc::CritScope crit(&decoder_->lock_);
  if (!decoder_->delegate_callback_) {
    return;
  }
  decoder_->delegate_callback_->Decoded(decodedImage, decode_time_ms, qp);
}

int32_t
QualityAnalyzingVideoDecoder::DecoderCallback::ReceivedDecodedReferenceFrame(
    const uint64_t pictureId) {
  rtc::CritScope crit(&decoder_->lock_);
  if (!decoder_->delegate_callback_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  return decoder_->delegate_callback_->ReceivedDecodedReferenceFrame(pictureId);
}

int32_t QualityAnalyzingVideoDecoder::DecoderCallback::ReceivedDecodedFrame(
    const uint64_t pictureId) {
  rtc::CritScope crit(&decoder_->lock_);
  if (!decoder_->delegate_callback_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  return decoder_->delegate_callback_->ReceivedDecodedFrame(pictureId);
}

void QualityAnalyzingVideoDecoder::DecoderCallback::OnFrameDecoded(
    VideoFrame* frame,
    absl::optional<int32_t> decode_time_ms,
    absl::optional<uint8_t> qp) {
  uint16_t frame_id;
  {
    rtc::CritScope crit(&decoder_->lock_);
    auto it = decoder_->timestamp_to_frame_id_.find(frame->timestamp());
    if (it == decoder_->timestamp_to_frame_id_.end()) {
      // Ensure, that we have info about this frame.
      RTC_LOG(WARNING) << "No frame id for frame with timestamp: "
                       << frame->timestamp();
      return;
    }
    frame_id = it->second;
    decoder_->timestamp_to_frame_id_.erase(it);
    decoder_->decoding_images_.erase(frame_id);
  }
  // Set frame id to the value, that was extracted from corresponding encoded
  // image.
  frame->set_id(frame_id);
  decoder_->analyzer_->OnFrameDecoded(*frame, decode_time_ms, qp);
}

QualityAnalyzingVideoDecoderFactory::QualityAnalyzingVideoDecoderFactory(
    std::unique_ptr<VideoDecoderFactory> delegate,
    IdGenerator<int>* id_generator,
    EncodedImageIdExtractor* extractor,
    VideoQualityAnalyzerInterface* analyzer)
    : delegate_(std::move(delegate)),
      id_generator_(id_generator),
      extractor_(extractor),
      analyzer_(analyzer) {}
QualityAnalyzingVideoDecoderFactory::~QualityAnalyzingVideoDecoderFactory() =
    default;

std::vector<SdpVideoFormat>
QualityAnalyzingVideoDecoderFactory::GetSupportedFormats() const {
  return delegate_->GetSupportedFormats();
}

std::unique_ptr<VideoDecoder>
QualityAnalyzingVideoDecoderFactory::CreateVideoDecoder(
    const SdpVideoFormat& format) {
  std::unique_ptr<VideoDecoder> decoder = delegate_->CreateVideoDecoder(format);
  return absl::make_unique<QualityAnalyzingVideoDecoder>(
      id_generator_->GetNextId(), std::move(decoder), extractor_, analyzer_);
}

std::unique_ptr<VideoDecoder>
QualityAnalyzingVideoDecoderFactory::LegacyCreateVideoDecoder(
    const SdpVideoFormat& format,
    const std::string& receive_stream_id) {
  std::unique_ptr<VideoDecoder> decoder =
      delegate_->LegacyCreateVideoDecoder(format, receive_stream_id);
  return absl::make_unique<QualityAnalyzingVideoDecoder>(
      id_generator_->GetNextId(), std::move(decoder), extractor_, analyzer_);
}

}  // namespace test
}  // namespace webrtc
