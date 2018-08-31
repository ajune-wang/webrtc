/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/fake_video_codec_factory.h"

#include "api/video/i420_buffer.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "common_video/include/video_frame.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace {

static const char kFakeCodecFactoryCodecName[] = "FakeCodec";

static const int kBitsPerByte = 8;

void PaintBuffer(webrtc::I420Buffer* buffer,
                 int value_y,
                 int value_u,
                 int value_v) {
  libyuv::I420Rect(
      buffer->MutableDataY(), buffer->StrideY(), buffer->MutableDataU(),
      buffer->StrideU(), buffer->MutableDataV(), buffer->StrideV(), 0, 0,
      buffer->width(), buffer->height(), value_y, value_u, value_v);
}

class FakeVideoEncoder : public webrtc::VideoEncoder {
 public:
  int32_t InitEncode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) override {
    memset(&codec_specific_, 0, sizeof(codec_specific_));
    codec_specific_.codecType = webrtc::kVideoCodecGeneric;
    frag_info_.VerifyAndAllocateFragmentationHeader(1);
    frag_info_.fragmentationOffset[0] = 0;
    frag_info_.fragmentationPlType[0] = 0;
    frag_info_.fragmentationTimeDiff[0] = 0;
    return WEBRTC_VIDEO_CODEC_OK;
  }
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override {
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
  }
  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }
  int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) override {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  int32_t SetRateAllocation(const webrtc::VideoBitrateAllocation& allocation,
                            uint32_t framerate) override {
    bytes_allocated_per_frame_ =
        allocation.get_sum_bps() / (framerate * kBitsPerByte);
    return WEBRTC_VIDEO_CODEC_OK;
  }
  int32_t Encode(const webrtc::VideoFrame& input_frame,
                 const webrtc::CodecSpecificInfo* codec_specific_info,
                 const std::vector<webrtc::FrameType>* frame_types) override {
    if (dummy_buffer_.size() < bytes_allocated_per_frame_)
      dummy_buffer_.resize(bytes_allocated_per_frame_, 0);

    encoded_image_._buffer = dummy_buffer_.data();
    encoded_image_._size = bytes_allocated_per_frame_;
    encoded_image_._length = bytes_allocated_per_frame_;
    encoded_image_._frameType = (*frame_types)[0];
    encoded_image_.SetTimestamp(input_frame.timestamp());
    encoded_image_.capture_time_ms_ = input_frame.render_time_ms();
    encoded_image_.rotation_ = input_frame.rotation();
    encoded_image_._encodedWidth = input_frame.width();
    encoded_image_._encodedHeight = input_frame.height();
    encoded_image_._completeFrame = true;

    frag_info_.fragmentationLength[0] = encoded_image_._length;

    callback_->OnEncodedImage(encoded_image_, &codec_specific_, &frag_info_);
    return WEBRTC_VIDEO_CODEC_OK;
  }

 private:
  webrtc::EncodedImage encoded_image_;
  webrtc::CodecSpecificInfo codec_specific_;
  webrtc::RTPFragmentationHeader frag_info_;
  std::vector<uint8_t> dummy_buffer_;
  size_t bytes_allocated_per_frame_ = 0;
  webrtc::EncodedImageCallback* callback_ = nullptr;
};

class FakeVideoDecoder : public webrtc::VideoDecoder {
 public:
  int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores) override {
    width_ = codec_settings->width;
    height_ = codec_settings->height;
    return WEBRTC_VIDEO_CODEC_OK;
  }
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override {
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
  }
  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 const webrtc::CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override {
    rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(width_, height_);
    PaintBuffer(buffer);
    webrtc::VideoFrame decoded_image =
        webrtc::VideoFrame::Builder()
            .set_video_frame_buffer(buffer)
            .set_timestamp_ms(0)
            .set_timestamp_rtp(input_image.Timestamp())
            .set_ntp_time_ms(input_image.ntp_time_ms_)
            .build();
    callback_->Decoded(decoded_image);
    return WEBRTC_VIDEO_CODEC_OK;
  }

 private:
  void PaintBuffer(webrtc::I420Buffer* buffer) {
    static int kNumColors = 4;
    buffer_count_ = (buffer_count_ + 1) % kNumColors;
    switch (buffer_count_) {
      case 0:
        ::PaintBuffer(buffer, 0, 128, 128);
        break;
      case 1:
        ::PaintBuffer(buffer, 64, 64, 255);
        break;
      case 2:
        ::PaintBuffer(buffer, 128, 64, 64);
        break;
      case 3:
      default:
        ::PaintBuffer(buffer, 64, 255, 128);
        break;
    }
  }

  webrtc::DecodedImageCallback* callback_ = nullptr;
  size_t width_ = 0;
  size_t height_ = 0;
  int buffer_count_ = 0;
};

}  // anonymous namespace

namespace webrtc {

FakeVideoEncoderFactory::FakeVideoEncoderFactory() = default;

// static
std::unique_ptr<VideoEncoder> FakeVideoEncoderFactory::CreateVideoEncoder() {
  return absl::make_unique<FakeVideoEncoder>();
}

std::vector<SdpVideoFormat> FakeVideoEncoderFactory::GetSupportedFormats()
    const {
  return std::vector<SdpVideoFormat>(
      1, SdpVideoFormat(kFakeCodecFactoryCodecName));
}

VideoEncoderFactory::CodecInfo FakeVideoEncoderFactory::QueryVideoEncoder(
    const SdpVideoFormat& format) const {
  return VideoEncoderFactory::CodecInfo{false, false};
}

std::unique_ptr<VideoEncoder> FakeVideoEncoderFactory::CreateVideoEncoder(
    const SdpVideoFormat& format) {
  return absl::make_unique<FakeVideoEncoder>();
}

FakeVideoDecoderFactory::FakeVideoDecoderFactory() = default;

// static
std::unique_ptr<VideoDecoder> FakeVideoDecoderFactory::CreateVideoDecoder() {
  return absl::make_unique<FakeVideoDecoder>();
}

std::vector<SdpVideoFormat> FakeVideoDecoderFactory::GetSupportedFormats()
    const {
  return std::vector<SdpVideoFormat>(
      1, SdpVideoFormat(kFakeCodecFactoryCodecName));
}

std::unique_ptr<VideoDecoder> FakeVideoDecoderFactory::CreateVideoDecoder(
    const SdpVideoFormat& format) {
  return absl::make_unique<FakeVideoDecoder>();
}

}  // namespace webrtc
