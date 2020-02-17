/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_async_sender_video.h"
#include <memory>

namespace webrtc {

RTPAsyncSenderVideo::RTPAsyncSenderVideo(
    const RTPSenderVideo::Config& config,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer)
    : RTPSenderVideo(config), frame_transformer_(frame_transformer) {
  // RTC_LOG(LS_ERROR) << "[webrtc sender] register callback.";
}

bool RTPAsyncSenderVideo::SendVideo(
    int payload_type,
    absl::optional<VideoCodecType> codec_type,
    uint32_t rtp_timestamp,
    int64_t capture_time_ms,
    const EncodedImage& encoded_image,
    const RTPFragmentationHeader* fragmentation,
    RTPVideoHeader video_header,
    absl::optional<int64_t> expected_retransmission_time_ms) {
  // RTC_LOG(LS_ERROR) << "[webrtc sender] transform image.";
  frame_transformer_->TransformFrame(
      std::make_unique<video_coding::RtpEncodedFrameObject>(
          encoded_image->GetEncodedData(), video_header, payload_type,
          codec_type, rtp_timestamp, capture_time_ms, fragmentation,
          expected_retransmission_time_ms));
  return true;
}

void RTPAsyncSenderVideo::OnTransformedFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  // RTC_LOG(LS_ERROR) << "[webrtc sender] handle transformed frame.";
  //  worker_queue_->PostTask([this, frame = std::move(frame)]() mutable {
  auto transformed_frame = std::unique_ptr<video_coding::RtpEncodedFrameObject>(
      static_cast<video_coding::RtpEncodedFrameObject*>(frame.release()));
  RTPSenderVideo::SendVideo(
      transformed_frame->PayloadType(), transformed_frame->codec_type(),
      transformed_frame->Timestamp(), transformed_frame->capture_time_ms(),
      transformed_frame->EncodedImage(),
      transformed_frame->fragmentation_header(),
      transformed_frame->video_header(),
      transformed_frame->expected_retransmission_time_ms());
  //  });
}

}  // namespace webrtc
