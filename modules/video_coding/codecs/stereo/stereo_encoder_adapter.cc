/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/stereo/include/stereo_encoder_adapter.h"

#include "common_video/include/video_frame.h"
#include "common_video/include/video_frame_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/include/module_common_types.h"

#include "rtc_base/keep_ref_until_done.h"
#include "rtc_base/logging.h"

namespace webrtc {

// Callback wrapper that helps distinguish returned results from |encoders_|
// instances.
class StereoEncoderAdapter::AdapterEncodedImageCallback
    : public webrtc::EncodedImageCallback {
 public:
  AdapterEncodedImageCallback(webrtc::StereoEncoderAdapter* adapter,
                              AlphaCodecStream stream_idx)
      : adapter_(adapter), stream_idx_(stream_idx) {}

  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override {
    if (!adapter_)
      return Result(Result::OK);
    return adapter_->OnEncodedImage(stream_idx_, encoded_image,
                                    codec_specific_info, fragmentation);
  }

 private:
  StereoEncoderAdapter* adapter_;
  const AlphaCodecStream stream_idx_;
};

StereoEncoderAdapter::StereoEncoderAdapter(
    VideoEncoderFactory* factory,
    const SdpVideoFormat& associated_format)
    : factory_(factory),
      associated_format_(associated_format),
      encoded_complete_callback_(nullptr),
      clock_(webrtc::Clock::GetRealTimeClock()),
      last_key_frame_ms_(0) {}

StereoEncoderAdapter::~StereoEncoderAdapter() {
  Release();
}

int StereoEncoderAdapter::InitEncode(const VideoCodec* inst,
                                     int number_of_cores,
                                     size_t max_payload_size) {
  const size_t buffer_size =
      CalcBufferSize(VideoType::kI420, inst->width, inst->height);
  stereo_dummy_planes_.resize(buffer_size);
  // It is more expensive to encode 0x00, so use 0x80 instead.
  std::fill(stereo_dummy_planes_.begin(), stereo_dummy_planes_.end(), 0x80);

  RTC_DCHECK_EQ(kVideoCodecStereo, inst->codecType);
  VideoCodec settings = *inst;
  settings.codecType = PayloadStringToCodecType(associated_format_.name);

  // Take over the key frame interval at adapter level, because we have to
  // sync the key frames for both sub-encoders.
  switch (settings.codecType) {
    case kVideoCodecVP8:
      key_frame_interval_ = settings.VP8()->keyFrameInterval;
      settings.VP8()->keyFrameInterval = 0;
      break;
    case kVideoCodecVP9:
      key_frame_interval_ = settings.VP9()->keyFrameInterval;
      settings.VP9()->keyFrameInterval = 0;
      break;
    case kVideoCodecH264:
      key_frame_interval_ = settings.H264()->keyFrameInterval;
      settings.H264()->keyFrameInterval = 0;
      break;
    default:
      break;
  }

  for (size_t i = 0; i < kAlphaCodecStreams; ++i) {
    std::unique_ptr<VideoEncoder> encoder =
        factory_->CreateVideoEncoder(associated_format_);
    const int rv =
        encoder->InitEncode(&settings, number_of_cores, max_payload_size);
    if (rv) {
      RTC_LOG(LS_ERROR) << "Failed to create stereo codec index " << i;
      return rv;
    }
    adapter_callbacks_.emplace_back(new AdapterEncodedImageCallback(
        this, static_cast<AlphaCodecStream>(i)));
    encoder->RegisterEncodeCompleteCallback(adapter_callbacks_.back().get());
    encoders_.emplace_back(std::move(encoder));
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int StereoEncoderAdapter::Encode(const VideoFrame& input_image,
                                 const CodecSpecificInfo* codec_specific_info,
                                 const std::vector<FrameType>* frame_types) {
  if (!encoded_complete_callback_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  std::vector<FrameType> frame_types2;
  int64_t now = clock_->TimeInMilliseconds();
  if (key_frame_interval_ > 0 &&
      now - last_key_frame_ms_ > key_frame_interval_ * 1000) {
    frame_types2.push_back(kVideoFrameKey);
    last_key_frame_ms_ = now;
  } else {
    frame_types2.push_back(kVideoFrameDelta);
  }
  const bool has_alpha = input_image.video_frame_buffer()->type() ==
                         VideoFrameBuffer::Type::kI420A;
  stashed_images_.emplace(
      std::piecewise_construct, std::forward_as_tuple(input_image.timestamp()),
      std::forward_as_tuple(picture_index_++,
                            has_alpha ? kAlphaCodecStreams : 1));

  // Encode YUV
  int rv = encoders_[kYUVStream]->Encode(input_image, codec_specific_info,
                                         &frame_types2);
  // If we do not receive an alpha frame, we send a single frame for this
  // |picture_index_|. The receiver will receive |frame_count| as 1 which
  // soecifies this case.
  if (rv || !has_alpha)
    return rv;

  // Encode AXX
  const I420ABufferInterface* yuva_buffer =
      input_image.video_frame_buffer()->GetI420A();
  rtc::scoped_refptr<I420BufferInterface> alpha_buffer =
      WrapI420Buffer(input_image.width(), input_image.height(),
                     yuva_buffer->DataA(), yuva_buffer->StrideA(),
                     stereo_dummy_planes_.data(), yuva_buffer->StrideU(),
                     stereo_dummy_planes_.data(), yuva_buffer->StrideV(),
                     rtc::KeepRefUntilDone(input_image.video_frame_buffer()));
  VideoFrame alpha_image(alpha_buffer, input_image.timestamp(),
                         input_image.render_time_ms(), input_image.rotation());
  rv = encoders_[kAXXStream]->Encode(alpha_image, codec_specific_info,
                                     &frame_types2);
  return rv;
}

int StereoEncoderAdapter::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  encoded_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int StereoEncoderAdapter::SetChannelParameters(uint32_t packet_loss,
                                               int64_t rtt) {
  for (auto& encoder : encoders_) {
    const int rv = encoder->SetChannelParameters(packet_loss, rtt);
    if (rv)
      return rv;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int StereoEncoderAdapter::SetRateAllocation(const BitrateAllocation& bitrate,
                                            uint32_t framerate) {
  for (auto& encoder : encoders_) {
    // TODO(emircan): |framerate| is used to calculate duration in encoder
    // instances. We report the total frame rate to keep real time for now.
    // Remove this after refactoring duration logic.
    const int rv = encoder->SetRateAllocation(
        bitrate, static_cast<uint32_t>(encoders_.size()) * framerate);
    if (rv)
      return rv;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int StereoEncoderAdapter::Release() {
  for (auto& encoder : encoders_) {
    const int rv = encoder->Release();
    if (rv)
      return rv;
  }
  encoders_.clear();
  adapter_callbacks_.clear();
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* StereoEncoderAdapter::ImplementationName() const {
  return "StereoEncoderAdapter";
}

EncodedImageCallback::Result StereoEncoderAdapter::OnEncodedImage(
    AlphaCodecStream stream_idx,
    const EncodedImage& encodedImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const RTPFragmentationHeader* fragmentation) {
  const auto& image_stereo_info_itr =
      stashed_images_.find(encodedImage._timeStamp);
  RTC_DCHECK(image_stereo_info_itr != stashed_images_.end());
  MultiplexImage& image_stereo_info = image_stereo_info_itr->second;
  const uint8_t frame_count = image_stereo_info.frame_count;

  // Incomplete case
  MultiplexImageComponent image_component;
  image_component.frame_index = stream_idx;
  image_component.codec_type =
      PayloadStringToCodecType(associated_format_.name);
  image_component.encoded_image = encodedImage;
  image_component.encoded_image._buffer = new uint8_t[encodedImage._length];
  std::memcpy(image_component.encoded_image._buffer, encodedImage._buffer,
              encodedImage._length);

  image_stereo_info.image_components.push_back(image_component);

  if (image_stereo_info.image_components.size() == frame_count) {
    // Complete case
    auto iter = stashed_images_.begin();
    while (iter != stashed_images_.end()) {
      // We have to send out those stashed frames, otherwise the delta frame
      // dependency chain is broken.
      EncodedImage combinedImage =
          MultiplexEncodedImagePacker::PackAndRelease(image_stereo_info);
      encoded_complete_callback_->OnEncodedImage(
          combinedImage, codecSpecificInfo, fragmentation);
      delete[] combinedImage._buffer;
      if (iter == image_stereo_info_itr)
        break;
      iter++;
    }

    stashed_images_.erase(stashed_images_.begin(),
                          std::next(image_stereo_info_itr, 1));
  }
  return EncodedImageCallback::Result(EncodedImageCallback::Result::OK);
}

}  // namespace webrtc
