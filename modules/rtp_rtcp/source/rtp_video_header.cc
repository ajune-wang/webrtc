/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_video_header.h"

#include "rtc_base/string_encode.h"

namespace webrtc {

RTPVideoHeader::RTPVideoHeader() : video_timing() {}
RTPVideoHeader::RTPVideoHeader(const RTPVideoHeader& other) = default;
RTPVideoHeader::~RTPVideoHeader() = default;

RTPVideoHeader::GenericDescriptorInfo::GenericDescriptorInfo() = default;
RTPVideoHeader::GenericDescriptorInfo::GenericDescriptorInfo(
    const GenericDescriptorInfo& other) = default;
RTPVideoHeader::GenericDescriptorInfo::~GenericDescriptorInfo() = default;

std::map<std::string, std::string> RTPVideoHeader::ToMap() const {
  std::map<std::string, std::string> map;
  if (generic.has_value()) {
    map["generic"] = "TODO";
  }
  map["frameType"] = VideoFrameTypeToString(frame_type);
  map["width"] = rtc::ToString(width);
  map["height"] = rtc::ToString(height);
  map["rotation"] = "TODO";
  map["contentType"] = "TODO";
  map["isFirstPacketInFrame"] = rtc::ToString(is_first_packet_in_frame);
  map["isLastPacketInFrame"] = rtc::ToString(is_last_packet_in_frame);
  map["isLastFrameInPicture"] = rtc::ToString(is_last_frame_in_picture);
  map["simulcastIdx"] = rtc::ToString(simulcastIdx);
  /*VideoCodecType codec = VideoCodecType::kVideoCodecGeneric;

  VideoPlayoutDelay playout_delay;
  VideoSendTiming video_timing;
  absl::optional<ColorSpace> color_space;
  // This field is meant for media quality testing purpose only. When enabled it
  // carries the webrtc::VideoFrame id field from the sender to the receiver.
  absl::optional<uint16_t> video_frame_tracking_id;
  RTPVideoTypeHeader video_type_header;

  // When provided, is sent as is as an RTP header extension according to
  // http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time.
  // Otherwise, it is derived from other relevant information.
  absl::optional<AbsoluteCaptureTime> absolute_capture_time;*/

  return map;
}

}  // namespace webrtc
