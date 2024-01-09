/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/h26x_packet_buffer_utils.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "api/video/video_frame_type.h"
#include "common_video/h264/h264_common.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/sequence_number_util.h"

namespace webrtc {
namespace video_coding {

void ProcessH264Packets(
    uint16_t seq_num,
    uint16_t start,
    size_t index,
    bool idr_only_keyframes_allowed,
    std::vector<std::unique_ptr<PacketBuffer::Packet>>& buffer,
    std::vector<std::unique_ptr<PacketBuffer::Packet>>& found_frames,
    std::set<uint16_t, DescendingSeqNumComp<uint16_t>>& missing_packets,
    std::set<uint16_t, DescendingSeqNumComp<uint16_t>>& received_padding) {
  RTC_DCHECK(buffer[index]->codec() == kVideoCodecH264 ||
             buffer[index]->codec() == kVideoCodecH265);
  uint16_t start_seq_num = seq_num;

  // Find the start index by searching backward until the packet with
  // the `frame_begin` flag is set.
  int start_index = index;
  size_t tested_packets = 0;
  int64_t frame_timestamp = buffer[start_index]->timestamp;

  // Identify H.264 keyframes by means of SPS, PPS, and IDR.
  bool has_h264_sps = false;
  bool has_h264_pps = false;
  bool has_h264_idr = false;
  bool is_h264_keyframe = false;
  int idr_width = -1;
  int idr_height = -1;

  while (true) {
    ++tested_packets;

    RTC_DCHECK_EQ(buffer[start_index]->codec(), kVideoCodecH264);
    const auto* h264_header = absl::get_if<RTPVideoHeaderH264>(
        &buffer[start_index]->video_header.video_type_header);
    if (!h264_header || h264_header->nalus_length >= kMaxNalusPerPacket) {
      return;
    }

    for (size_t j = 0; j < h264_header->nalus_length; ++j) {
      if (h264_header->nalus[j].type == H264::NaluType::kSps) {
        has_h264_sps = true;
      } else if (h264_header->nalus[j].type == H264::NaluType::kPps) {
        has_h264_pps = true;
      } else if (h264_header->nalus[j].type == H264::NaluType::kIdr) {
        has_h264_idr = true;
      }
    }
    if ((idr_only_keyframes_allowed && has_h264_idr && has_h264_sps &&
         has_h264_pps) ||
        (!idr_only_keyframes_allowed && has_h264_idr)) {
      is_h264_keyframe = true;
      // Store the resolution of key frame which is the packet with
      // smallest index and valid resolution; typically its IDR or SPS
      // packet; there may be packet preceeding this packet, IDR's
      // resolution will be applied to them.
      if (buffer[start_index]->width() > 0 &&
          buffer[start_index]->height() > 0) {
        idr_width = buffer[start_index]->width();
        idr_height = buffer[start_index]->height();
      }
    }

    if (tested_packets == buffer.size())
      break;

    start_index = start_index > 0 ? start_index - 1 : buffer.size() - 1;

    // In the case of H264 we don't have a frame_begin bit (yes,
    // `frame_begin` might be set to true but that is a lie). So instead
    // we traverese backwards as long as we have a previous packet and
    // the timestamp of that packet is the same as this one. This may cause
    // the PacketBuffer to hand out incomplete frames.
    // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=7106
    if (buffer[start_index] == nullptr ||
        buffer[start_index]->timestamp != frame_timestamp) {
      break;
    }

    --start_seq_num;
  }

  // Warn if this is an unsafe frame.
  if (has_h264_idr && (!has_h264_sps || !has_h264_pps)) {
    RTC_LOG(LS_WARNING) << "Received H.264-IDR frame "
                           "(SPS: "
                        << has_h264_sps << ", PPS: " << has_h264_pps
                        << "). Treating as "
                        << (idr_only_keyframes_allowed ? "delta" : "key")
                        << " frame since WebRTC-SpsPpsIdrIsH264Keyframe is "
                        << (idr_only_keyframes_allowed ? "enabled."
                                                       : "disabled");
  }

  // Now that we have decided whether to treat this frame as a key frame
  // or delta frame in the frame buffer, we update the field that
  // determines if the RtpFrameObject is a key frame or delta frame.
  const size_t first_packet_index = start_seq_num % buffer.size();
  if (is_h264_keyframe) {
    buffer[first_packet_index]->video_header.frame_type =
        VideoFrameType::kVideoFrameKey;
    if (idr_width > 0 && idr_height > 0) {
      // IDR frame was finalized and we have the correct resolution for
      // IDR; update first packet to have same resolution as IDR.
      buffer[first_packet_index]->video_header.width = idr_width;
      buffer[first_packet_index]->video_header.height = idr_height;
    }
  } else {
    buffer[first_packet_index]->video_header.frame_type =
        VideoFrameType::kVideoFrameDelta;
  }

  // If this is not a keyframe, make sure there are no gaps in the packet
  // sequence numbers up until this point.
  if (!is_h264_keyframe &&
      missing_packets.upper_bound(start_seq_num) != missing_packets.begin()) {
    return;
  }

  const uint16_t end_seq_num = seq_num + 1;
  // Use uint16_t type to handle sequence number wrap around case.
  uint16_t num_packets = end_seq_num - start_seq_num;
  found_frames.reserve(found_frames.size() + num_packets);
  for (uint16_t i = start_seq_num; i != end_seq_num; ++i) {
    std::unique_ptr<PacketBuffer::Packet>& packet = buffer[i % buffer.size()];
    RTC_DCHECK(packet);
    RTC_DCHECK_EQ(i, packet->seq_num);
    // Ensure frame boundary flags are properly set.
    packet->video_header.is_first_packet_in_frame = (i == start_seq_num);
    packet->video_header.is_last_packet_in_frame = (i == seq_num);
    found_frames.push_back(std::move(packet));
  }

  missing_packets.erase(missing_packets.begin(),
                        missing_packets.upper_bound(seq_num));
  received_padding.erase(received_padding.lower_bound(start),
                         received_padding.upper_bound(seq_num));
}

}  // namespace video_coding
}  // namespace webrtc
