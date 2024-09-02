/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/black_frame_decoder.h"

#include <cstdint>
#include <optional>

#include "api/array_view.h"
#include "api/scoped_refptr.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/sps_parser.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/qp_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "rtc_base/logging.h"
#ifdef WEBRTC_CHROMIUM_BUILD
#include "third_party/dav1d/libdav1d/include/dav1d/dav1d.h"
#endif

namespace webrtc {

BlackFrameDecoder::BlackFrameDecoder(SdpVideoFormat format)
    : width_(2),
      height_(2),
      qp_(0),
      type_(PayloadStringToCodecType(format.name)) {}

bool BlackFrameDecoder::Configure(const Settings& settings) {
  return true;
}

int32_t BlackFrameDecoder::Decode(const webrtc::EncodedImage& input_image,
                                  bool missing_frames,
                                  int64_t render_time_ms) {
  // For keyframes store the size and reuse in delta frames.
  if (input_image._frameType == VideoFrameType::kVideoFrameKey) {
    QpParser qp_parser;
    std::optional<uint32_t> qp =
        qp_parser.Parse(type_, 0, input_image.data(), input_image.size());
    qp_ = qp.has_value() ? *qp : 0;
    switch (type_) {
      case VideoCodecType::kVideoCodecVP8: {
        if (input_image.size() < 10) {
          return WEBRTC_VIDEO_CODEC_ERROR;
        }
        // See modules/rtp_rtcp/source/video_rtp_depacketizer_vp8.cc;l=189
        width_ =
            ((input_image.data()[7] << 8) + input_image.data()[6]) & 0x3FFF;
        height_ =
            ((input_image.data()[9] << 8) + input_image.data()[8]) & 0x3FFF;
      } break;
      case VideoCodecType::kVideoCodecVP9: {
        auto vp9 = ParseUncompressedVp9Header(rtc::ArrayView<const uint8_t>(
            input_image.data(), input_image.size()));
        if (!vp9) {
          return WEBRTC_VIDEO_CODEC_ERROR;
        }
        width_ = vp9->frame_width;
        height_ = vp9->frame_height;
      } break;
      case VideoCodecType::kVideoCodecH264: {
        if (input_image.size() <
            H264::kNaluLongStartSequenceSize + H264::kNaluTypeSize) {
          return WEBRTC_VIDEO_CODEC_ERROR;
        }
        auto sps = SpsParser::ParseSps(
            input_image.data() + H264::kNaluLongStartSequenceSize +
                H264::kNaluTypeSize,
            input_image.size() -
                (H264::kNaluLongStartSequenceSize + H264::kNaluTypeSize));
        if (!sps.has_value()) {
          return WEBRTC_VIDEO_CODEC_ERROR;
        }
        width_ = sps->width;
        height_ = sps->height;
      } break;
#ifdef WEBRTC_CHROMIUM_BUILD
      case VideoCodecType::kVideoCodecAV1: {
        Dav1dSequenceHeader header;
        if (0 != dav1d_parse_sequence_header(&header, input_image.data(),
                                             input_image.size())) {
          return WEBRTC_VIDEO_CODEC_ERROR;
        }
        width_ = header.max_width;
        height_ = header.max_height;
      } break;
#endif
      default:
        RTC_LOG(LS_ERROR) << "Unsupported codec "
                          << CodecTypeToPayloadString(type_)
                          << ", setting frame size to 2x2 pixels.";
        width_ = 2;
        height_ = 2;
        break;
    }
  }
  webrtc::VideoFrame video_frame =
      CreateFrame(width_, height_, input_image.RtpTimestamp());
  decode_complete_callback_->Decoded(video_frame, std::nullopt, qp_);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t BlackFrameDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  decode_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t BlackFrameDecoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* BlackFrameDecoder::ImplementationName() const {
  return "BlackFrameDecoder";
}

webrtc::VideoFrame BlackFrameDecoder::CreateFrame(int width,
                                                  int height,
                                                  uint32_t timestamp) {
  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(width, height);
  webrtc::I420Buffer::SetBlack(buffer.get());
  return webrtc::VideoFrame::Builder()
      .set_video_frame_buffer(buffer)
      .set_timestamp_rtp(timestamp)
      .build();
}

}  // namespace webrtc
