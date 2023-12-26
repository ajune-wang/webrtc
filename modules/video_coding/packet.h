/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_PACKET_H_
#define MODULES_VIDEO_CODING_PACKET_H_

#include <memory>
#include <queue>
#include <set>
#include <vector>

#include "absl/base/attributes.h"
#include "api/rtp_packet_info.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace video_coding {

struct Packet {
  Packet() = default;
  Packet(const RtpPacketReceived& rtp_packet,
         const RTPVideoHeader& video_header);
  Packet(const Packet&) = delete;
  Packet(Packet&&) = delete;
  Packet& operator=(const Packet&) = delete;
  Packet& operator=(Packet&&) = delete;
  ~Packet() = default;

  VideoCodecType codec() const { return video_header.codec; }
  int width() const { return video_header.width; }
  int height() const { return video_header.height; }

  bool is_first_packet_in_frame() const {
    return video_header.is_first_packet_in_frame;
  }
  bool is_last_packet_in_frame() const {
    return video_header.is_last_packet_in_frame;
  }

  // If all its previous packets have been inserted into the packet buffer.
  // Set and used internally by the PacketBuffer.
  bool continuous = false;
  bool marker_bit = false;
  uint8_t payload_type = 0;
  uint16_t seq_num = 0;
  uint32_t timestamp = 0;
  int times_nacked = -1;

  rtc::CopyOnWriteBuffer video_payload;
  RTPVideoHeader video_header;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_PACKET_H_
