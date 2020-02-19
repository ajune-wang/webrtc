/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/rtp_encoded_frame_object.h"

namespace webrtc {
namespace video_coding {

RtpEncodedFrameObject::RtpEncodedFrameObject(
    rtc::scoped_refptr<EncodedImageBufferInterface> encoded_data,
    RTPVideoHeader video_header,
    int payload_type,
    absl::optional<VideoCodecType> codec_type,
    uint32_t rtp_timestamp,
    int64_t capture_time_ms,
    const RTPFragmentationHeader* fragmentation,
    absl::optional<int64_t> expected_retransmission_time_ms)
    : video_header_(video_header),
      codec_type_(codec_type),
      expected_retransmission_time_ms_(expected_retransmission_time_ms) {
  SetEncodedData(encoded_data);
  _payloadType = payload_type;
  SetTimestamp(rtp_timestamp);
  capture_time_ms_ = capture_time_ms;
  if (fragmentation)
    fragmentation_header_->CopyFrom(*fragmentation);
}

RtpEncodedFrameObject::~RtpEncodedFrameObject() {}

int64_t RtpEncodedFrameObject::ReceivedTime() const {
  return 0;
}

int64_t RtpEncodedFrameObject::RenderTime() const {
  return _renderTimeMs;
}

RTPVideoHeader RtpEncodedFrameObject::video_header() {
  return video_header_;
}

const absl::optional<VideoCodecType>& RtpEncodedFrameObject::codec_type()
    const {
  return codec_type_;
}

int64_t RtpEncodedFrameObject::capture_time_ms() const {
  return capture_time_ms_;
}

RTPFragmentationHeader* RtpEncodedFrameObject::fragmentation_header() {
  return fragmentation_header_.get();
}

const absl::optional<int64_t>&
RtpEncodedFrameObject::expected_retransmission_time_ms() const {
  return expected_retransmission_time_ms_;
}

}  // namespace video_coding
}  // namespace webrtc
