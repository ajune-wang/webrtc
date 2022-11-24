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

bool RTPVideoHeader::FromMap(const std::map<std::string, std::string>& cmap) {
  // Non-const version for operator[] convenience...
  std::map<std::string, std::string> map = cmap;

  bool success = true;
  if (map.contains("frameType")) {
    success &= VideoFrameTypeFromString(map["frameType"], &frame_type);
  }
  if (map.contains("width")) {
    success &= rtc::FromString<uint16_t>(map["width"], &width);
  }
  if (map.contains("height")) {
    success &= rtc::FromString<uint16_t>(map["height"], &height);
  }
  if (map.contains("rotation")) {
    success &= VideoRotationFromString(map["rotation"], &rotation);
  }
  if (map.contains("contentType")) {
    success &= rtc::FromString<uint8_t>(
        map["contentType"], reinterpret_cast<uint8_t*>(&content_type));
  }
  if (map.contains("isFirstPacketInFrame")) {
    success &=
        rtc::FromString(map["isFirstPacketInFrame"], &is_first_packet_in_frame);
  }
  if (map.contains("isLastPacketInFrame")) {
    success &=
        rtc::FromString(map["isLastPacketInFrame"], &is_last_packet_in_frame);
  }
  if (map.contains("isLastFrameInPicture")) {
    success &=
        rtc::FromString(map["isLastFrameInPicture"], &is_last_frame_in_picture);
  }
  if (map.contains("simulcastIdx")) {
    success &= rtc::FromString<uint8_t>(map["simulcastIdx"], &simulcastIdx);
  }
  if (map.contains("codec")) {
    success &= VideoCodecTypeFromString(map["codec"], &codec);
  }

  if (codec != VideoCodecType::kVideoCodecVP8) {
    success = false;
  } else {
    RTPVideoHeaderVP8& vp8_header =
        absl::get<RTPVideoHeaderVP8>(video_type_header);
    if (map.contains("videoTypeHeader.nonReference")) {
      success &= rtc::FromString(map["videoTypeHeader.nonReference"],
                                 &vp8_header.nonReference);
    }
    if (map.contains("videoTypeHeader.pictureId")) {
      success &= rtc::FromString<int16_t>(map["videoTypeHeader.pictureId"],
                                          &vp8_header.pictureId);
    }
    if (map.contains("videoTypeHeader.tl0PicIdx")) {
      success &= rtc::FromString<int16_t>(map["videoTypeHeader.tl0PicIdx"],
                                          &vp8_header.tl0PicIdx);
    }
    if (map.contains("videoTypeHeader.temporalIdx")) {
      success &= rtc::FromString<uint8_t>(map["videoTypeHeader.temporalIdx"],
                                          &vp8_header.temporalIdx);
    }
    if (map.contains("videoTypeHeader.layerSync")) {
      success &= rtc::FromString(map["videoTypeHeader.layerSync"],
                                 &vp8_header.layerSync);
    }
    if (map.contains("videoTypeHeader.keyIdx")) {
      success &= rtc::FromString<int>(map["videoTypeHeader.keyIdx"],
                                      &vp8_header.keyIdx);
    }
    if (map.contains("videoTypeHeader.partitionId")) {
      success &= rtc::FromString<int>(map["videoTypeHeader.partitionId"],
                                      &vp8_header.partitionId);
    }
    if (map.contains("videoTypeHeader.beginningOfPartition")) {
      success &= rtc::FromString(map["videoTypeHeader.beginningOfPartition"],
                                 &vp8_header.beginningOfPartition);
    }
  }

  return success;
}

}  // namespace webrtc
