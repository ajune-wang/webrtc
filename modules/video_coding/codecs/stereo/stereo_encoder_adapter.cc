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

// Holds the encoded image info.
struct StereoEncoderAdapter::ImageStereoInfo {
  ImageStereoInfo(uint16_t picture_index, uint8_t frame_count,
                  bool requesting_key)
      : picture_index(picture_index),
        frame_count(frame_count),
        encoded_count(0),
        requesting_key(requesting_key),
        stream_idx(-1) {}
  uint16_t picture_index;
  uint8_t frame_count;
  uint8_t encoded_count;
  bool requesting_key;
  int stream_idx;
  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  RTPFragmentationHeader frag_header;

 private:
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(ImageStereoInfo);
};

StereoEncoderAdapter::StereoEncoderAdapter(
    VideoEncoderFactory* external_factory,
    VideoEncoderFactory* internal_factory,
    const SdpVideoFormat& associated_format)
    : external_factory_(external_factory),
      internal_factory_(internal_factory),
      associated_format_(associated_format),
      encoded_complete_callback_(nullptr) {
  associated_codec_info_ = external_factory_ -> QueryVideoEncoder(
      associated_format);
  RTC_LOG(LS_ERROR)<< "StereoEncoderAdapter HW Support "
      << associated_format.name << " = "
      << associated_codec_info_.is_hardware_accelerated;
}

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
  switch(settings.codecType) {
    case kVideoCodecVP8:
      key_interval_ = settings.VP8()->keyFrameInterval;
      settings.VP8()->keyFrameInterval = 0;
      break;
    case kVideoCodecVP9:
      key_interval_ = settings.VP9()->keyFrameInterval;
      settings.VP9()->keyFrameInterval = 0;
      break;
    case kVideoCodecH264:
      key_interval_ = settings.H264()->keyFrameInterval;
      settings.H264()->keyFrameInterval = 0;
      break;
    default:
      break;
  }

  if (key_interval_ == 0) key_interval_ = 5;

  VideoEncoderFactory* factory = external_factory_;
  for (size_t i = 0; i < kAlphaCodecStreams; ++i) {
    std::unique_ptr<VideoEncoder> encoder =
        factory->CreateVideoEncoder(associated_format_);
    factory = internal_factory_;
    const int rv =
        encoder->InitEncode(&settings, number_of_cores, max_payload_size);
    RTC_LOG(LS_ERROR)<<"StereoEncoderAdapter::InitEncode Implementation Name: "
        <<encoder->ImplementationName();
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

  // TODO: use the key_interval_ to determine key frame frequency.
  frame_count_ = (frame_count_+1)%100;
  std::vector<FrameType> frame_types2;
  key_requesting_ = key_requesting_ || frame_count_ == 0;

  if (key_requesting_) {
    frame_types2.push_back(kVideoFrameKey);
  } else {
    frame_types2.push_back(kVideoFrameDelta);
  }

  const bool has_software_alpha = input_image.video_frame_buffer()->type() ==
                         VideoFrameBuffer::Type::kI420A;

  rtc::scoped_refptr<I420BufferInterface> alpha_buffer = input_image.video_frame_buffer()->MaskI420();

  const bool has_hardware_alpha = input_image.video_frame_buffer()->type() ==
                         VideoFrameBuffer::Type::kNative && (alpha_buffer != nullptr);

  const bool has_alpha = has_software_alpha || has_hardware_alpha;

  image_stereo_info_.emplace(
      std::piecewise_construct, std::forward_as_tuple(input_image.timestamp()),
      std::forward_as_tuple(picture_index_++,
                            has_alpha ? kAlphaCodecStreams : 1,
                            key_requesting_));

  key_requesting_ = false;

  // Encode YUV
  int rv = encoders_[kYUVStream]->Encode(input_image, codec_specific_info,
                                         &frame_types2);
  // If we do not receive an alpha frame, we send a single frame for this
  // |picture_index_|. The receiver will receive |frame_count| as 1 which
  // soecifies this case.
  if (rv || !has_alpha)
    return rv;

  // Encode AXX
  if (has_software_alpha) {
    const I420ABufferInterface* yuva_buffer =
        input_image.video_frame_buffer()->GetI420A();
    alpha_buffer =
        WrapI420Buffer(input_image.width(), input_image.height(),
                       yuva_buffer->DataA(), yuva_buffer->StrideA(),
                       stereo_dummy_planes_.data(), yuva_buffer->StrideU(),
                       stereo_dummy_planes_.data(), yuva_buffer->StrideV(),
                       rtc::KeepRefUntilDone(input_image.video_frame_buffer()));
  }
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

bool StereoEncoderAdapter::SupportsNativeHandle() const {
  return associated_codec_info_.is_hardware_accelerated;
}

EncodedImageCallback::Result StereoEncoderAdapter::OnEncodedImage(
    AlphaCodecStream stream_idx,
    const EncodedImage& encodedImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const RTPFragmentationHeader* fragmentation) {
  const VideoCodecType associated_codec_type = codecSpecificInfo->codecType;
  const auto& image_stereo_info_itr =
      image_stereo_info_.find(encodedImage._timeStamp);
  RTC_DCHECK(image_stereo_info_itr != image_stereo_info_.end());
  ImageStereoInfo& image_stereo_info = image_stereo_info_itr->second;
  const uint8_t frame_count = image_stereo_info.frame_count;
  const uint16_t picture_index = image_stereo_info.picture_index;
  ++image_stereo_info.encoded_count;

  if (image_stereo_info.requesting_key &&
      encodedImage._frameType != kVideoFrameKey) {
    key_requesting_ = true;
  }

  if (image_stereo_info.encoded_count < image_stereo_info.frame_count) {
    // Incomplete case
    image_stereo_info.stream_idx = stream_idx;
    image_stereo_info.encoded_image = encodedImage;
    image_stereo_info.encoded_image._buffer = new uint8_t[encodedImage._size];
    std::memcpy(image_stereo_info.encoded_image._buffer,
                encodedImage._buffer, encodedImage._size);
    image_stereo_info.codec_info = *codecSpecificInfo;
    image_stereo_info.frag_header.CopyFrom(*fragmentation);
  }
  else  {
    // Complete case
    auto iter = image_stereo_info_.begin();
    while (iter != image_stereo_info_.end() && iter != image_stereo_info_itr){
      if (iter->second.requesting_key) key_requesting_ = true;

      // We have to send out those stashed frames, otherwise the delta frame
      // dependency chain is broken.
      if (iter->second.stream_idx != -1){
        CodecSpecificInfo codec_info = image_stereo_info.codec_info;
        codec_info.codecType = kVideoCodecStereo;
        codec_info.codec_name = "stereo";
        codec_info.codecSpecific.stereo.associated_codec_type = associated_codec_type;
        codec_info.codecSpecific.stereo.indices.frame_index = iter->second.stream_idx;
        codec_info.codecSpecific.stereo.indices.frame_count = iter->second.frame_count;
        codec_info.codecSpecific.stereo.indices.picture_index = iter->second.picture_index;

        encoded_complete_callback_->OnEncodedImage(iter->second.encoded_image,
                                                   &codec_info,
                                                   &iter->second.frag_header);
        delete[] iter->second.encoded_image._buffer;
      }
      iter++;
    }

    if (frame_count > 1) {
      CodecSpecificInfo codec_info = image_stereo_info.codec_info;
      codec_info.codecType = kVideoCodecStereo;
      codec_info.codec_name = "stereo";
      codec_info.codecSpecific.stereo.associated_codec_type = associated_codec_type;
      codec_info.codecSpecific.stereo.indices.frame_index = kCombinedStream;
      codec_info.codecSpecific.stereo.indices.frame_count = frame_count;
      codec_info.codecSpecific.stereo.indices.picture_index = picture_index;

      if (combined_image_._buffer) delete[] combined_image_._buffer;

      const EncodedImage* yuv_image, *alpha_image;
      if (stream_idx == kYUVStream) {
        yuv_image = &encodedImage;
        alpha_image = &image_stereo_info.encoded_image;
      }
      else {
        alpha_image = &encodedImage;
        yuv_image = &image_stereo_info.encoded_image;
      }

      if (alpha_image->_encodedWidth != yuv_image->_encodedWidth ||
          alpha_image->_encodedHeight != yuv_image->_encodedHeight) {
        RTC_LOG(LS_ERROR)<<"Image Size Differ";
      }

      if (alpha_image->_frameType != yuv_image->_frameType) {
        RTC_LOG(LS_ERROR)<<"Image Type Differ";
      }

      combined_image_ = *yuv_image;
      combined_image_._buffer = new uint8_t[yuv_image->_size +
                                            alpha_image->_size];
      //RTC_LOG(LS_ERROR)<<"YUV Size"<<yuv_image->_size<<"  "<<yuv_image->_length;
      combined_image_._size = yuv_image->_size +
          alpha_image->_size;
      combined_image_._length = yuv_image->_size +
          alpha_image->_length;

      codec_info.codecSpecific.stereo.indices.yuv_size = yuv_image->_size;
      codec_info.codecSpecific.stereo.indices.yuv_length = yuv_image->_length;

      std::memcpy(combined_image_._buffer,
                  yuv_image->_buffer, yuv_image->_size);
      std::memcpy(combined_image_._buffer + yuv_image->_size,
                  alpha_image->_buffer, alpha_image->_size);

      encoded_complete_callback_->OnEncodedImage(combined_image_,
                                                 &codec_info,
                                                 &image_stereo_info.frag_header);

    }
    else {
      CodecSpecificInfo codec_info = *codecSpecificInfo;
      codec_info.codecType = kVideoCodecStereo;
      codec_info.codec_name = "stereo";
      codec_info.codecSpecific.stereo.associated_codec_type = associated_codec_type;
      codec_info.codecSpecific.stereo.indices.frame_index = stream_idx;
      codec_info.codecSpecific.stereo.indices.frame_count = frame_count;
      codec_info.codecSpecific.stereo.indices.picture_index = picture_index;

      encoded_complete_callback_->OnEncodedImage(encodedImage, &codec_info,
                                                 fragmentation);

    }

    image_stereo_info_.erase(image_stereo_info_.begin(),
                             std::next(image_stereo_info_itr, 1));
  }


  return EncodedImageCallback::Result(EncodedImageCallback::Result::OK);
}

}  // namespace webrtc
