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
  // if (generic.has_value()) {
  //   map["generic.frameId"] = rtc::ToString(frame_id);
  //   map["generic.spatialIndex"] = rtc::ToString(spatial_index);
  //   map["generic.temporalIndex"] = rtc::ToString(temporal_index);
  //   map["generic.decodeTargetIndication"] = "<todo>";
  //   map["generic.dependencies"] = "<todo>";
  //   map["generic.chainDiffs"] = "<todo>";
  //   map["generic.activeDecodeTargets"] = "<todo>";
  // }
  map["frameType"] = VideoFrameTypeToString(frame_type);
  map["width"] = rtc::ToString(width);
  map["height"] = rtc::ToString(height);
  map["rotation"] = VideoRotationToString(rotation);
  map["contentType"] = rtc::ToString(static_cast<uint8_t>(content_type));
  map["isFirstPacketInFrame"] = rtc::ToString(is_first_packet_in_frame);
  map["isLastPacketInFrame"] = rtc::ToString(is_last_packet_in_frame);
  map["isLastFrameInPicture"] = rtc::ToString(is_last_frame_in_picture);
  map["simulcastIdx"] = rtc::ToString(simulcastIdx);
  map["codec"] = VideoCodecTypeToString(codec);
  // map["playoutDelay"] = "<todo>";
  // map["videoTiming"] = "<todo>";
  // map["colorSpace"] = "<todo>";
  // map["videoFrameTrackingId"] = "<todo>";
  // map["absoluteCaptureTime"] = "<todo>";

  if (codec != VideoCodecType::kVideoCodecVP8) {
    map["videoTypeHeader"] = "UNKNOWN, ONLY VP8 IS CURENTLY SUPPORTED";
  } else {
    const RTPVideoHeaderVP8& vp8_header =
        absl::get<RTPVideoHeaderVP8>(video_type_header);
    map["videoTypeHeader.nonReference"] =
        rtc::ToString(vp8_header.nonReference);
    map["videoTypeHeader.pictureId"] = rtc::ToString(vp8_header.pictureId);
    map["videoTypeHeader.tl0PicIdx"] = rtc::ToString(vp8_header.tl0PicIdx);
    map["videoTypeHeader.temporalIdx"] = rtc::ToString(vp8_header.temporalIdx);
    map["videoTypeHeader.layerSync"] = rtc::ToString(vp8_header.layerSync);
    map["videoTypeHeader.keyIdx"] = rtc::ToString(vp8_header.keyIdx);
    map["videoTypeHeader.partitionId"] = rtc::ToString(vp8_header.partitionId);
    map["videoTypeHeader.beginningOfPartition"] =
        rtc::ToString(vp8_header.beginningOfPartition);
  }

  return map;
}

}  // namespace webrtc
