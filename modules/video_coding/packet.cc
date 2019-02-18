/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/packet.h"

#include "api/rtp_headers.h"
#include "modules/include/module_common_types.h"

namespace webrtc {

VCMPacket::VCMPacket()
    : payloadType(0),
      timestamp(0),
      ntp_time_ms_(0),
      seqNum(0),
      dataPtr(NULL),
      sizeBytes(0),
      markerBit(false),
      timesNacked(-1),
      frameType(kEmptyFrame),
      codec(kVideoCodecGeneric),
      is_first_packet_in_frame(false),
      is_last_packet_in_frame(false),
      completeNALU(kNaluUnset),
      insertStartCode(false),
      width(0),
      height(0),
      video_header(),
      receive_time_ms(0) {
  video_header.playout_delay = {-1, -1};
}

VCMPacket::VCMPacket(const uint8_t* ptr,
                     size_t size,
                     const WebRtcRTPHeader& rtpHeader)
    : VCMPacket(ptr,
                size,
                rtpHeader.header,
                rtpHeader.video_header(),
                rtpHeader.frameType,
                rtpHeader.ntp_time_ms) {}

VCMPacket::VCMPacket(const uint8_t* ptr,
                     size_t size,
                     const RTPHeader& rtp_header,
                     const RTPVideoHeader& videoHeader,
                     FrameType frame_type,
                     int64_t ntp_time_ms)
    : payloadType(rtp_header.payloadType),
      timestamp(rtp_header.timestamp),
      ntp_time_ms_(ntp_time_ms),
      seqNum(rtp_header.sequenceNumber),
      dataPtr(ptr),
      sizeBytes(size),
      markerBit(rtp_header.markerBit),
      timesNacked(-1),
      frameType(frame_type),
      codec(videoHeader.codec),
      is_first_packet_in_frame(videoHeader.is_first_packet_in_frame),
      is_last_packet_in_frame(videoHeader.is_last_packet_in_frame),
      completeNALU(kNaluIncomplete),
      insertStartCode(videoHeader.codec == kVideoCodecH264 &&
                      videoHeader.is_first_packet_in_frame),
      width(videoHeader.width),
      height(videoHeader.height),
      video_header(videoHeader) {
  if (is_first_packet_in_frame && markerBit) {
    completeNALU = kNaluComplete;
  } else if (is_first_packet_in_frame) {
    completeNALU = kNaluStart;
  } else if (markerBit) {
    completeNALU = kNaluEnd;
  } else {
    completeNALU = kNaluIncomplete;
  }

  if (markerBit) {
    video_header.rotation = videoHeader.rotation;
  }
  // Playout decisions are made entirely based on first packet in a frame.
  if (is_first_packet_in_frame) {
    video_header.playout_delay = videoHeader.playout_delay;
  } else {
    video_header.playout_delay = {-1, -1};
  }
}

VCMPacket::~VCMPacket() = default;

}  // namespace webrtc
