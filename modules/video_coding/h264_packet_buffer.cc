/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/h264_packet_buffer.h"

#include <string.h>

#include <algorithm>
#include <cstdint>
// #include <limits>
// #include <utility>
// #include <vector>

// #include "absl/container/inlined_vector.h"
#include "api/array_view.h"
#include "api/rtp_packet_info.h"
#include "api/video/video_frame_type.h"
#include "common_video/h264/h264_common.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/mod_ops.h"

namespace webrtc {
namespace {
const uint8_t kStartCode[] = {0, 0, 0, 1};
const int kSBit = 0x80;

int EucMod(int64_t n, int64_t div) {
  return (n %= div) < 0 ? n + div : n;
}

bool BeginningOfIdr(const H264PacketBuffer::Packet& packet) {
  auto& h264_header =
      absl::get<RTPVideoHeaderH264>(packet.video_header.video_type_header);
  const bool contains_idr_nalu = absl::c_any_of(
      rtc::MakeArrayView(h264_header.nalus, h264_header.nalus_length),
      [](const auto& nalu_info) {
        return nalu_info.type == H264::NaluType::kIdr;
      });
  switch (h264_header.packetization_type) {
    case kH264StapA:
    case kH264SingleNalu: {
      return contains_idr_nalu;
    }
    case kH264FuA: {
      return contains_idr_nalu && packet.video_payload.cdata()[1] & kSBit;
    }
  }
}

bool HasSps(const H264PacketBuffer::Packet& packet) {
  auto& h264_header =
      absl::get<RTPVideoHeaderH264>(packet.video_header.video_type_header);
  return absl::c_any_of(
      rtc::MakeArrayView(h264_header.nalus, h264_header.nalus_length),
      [](const auto& nalu_info) {
        return nalu_info.type == H264::NaluType::kSps;
      });
}

rtc::CopyOnWriteBuffer FixVideoPayload(rtc::CopyOnWriteBuffer payload,
                                       const RTPVideoHeader& video_header) {
  const auto& h264_header =
      absl::get<RTPVideoHeaderH264>(video_header.video_type_header);

  rtc::CopyOnWriteBuffer res;
  switch (h264_header.packetization_type) {
    case kH264StapA: {
      const uint8_t* payload_end = payload.cdata() + payload.size();
      const uint8_t* nalu_ptr = payload.cdata() + 1;
      while (nalu_ptr < payload_end - 1) {
        // The first two bytes describe the length of the segment, where a
        // segment is the nalu type plus nalu payload.
        uint16_t segment_length = nalu_ptr[0] << 8 | nalu_ptr[1];
        // TODO: check segment length
        nalu_ptr += 2;
        res.AppendData(kStartCode);
        res.AppendData(nalu_ptr, segment_length);
        nalu_ptr += segment_length;
      }
      return res;
    }

    case kH264FuA: {
      // TODO: fix names
      const int kFuAHeaderSize = 2;
      const int kTypeMask = 0x1F;

      // Only the first fragment includes the start code and nal type.
      if (payload.cdata()[1] & kSBit) {
        res.AppendData(kStartCode);
        uint8_t original_nal_type = payload.cdata()[1] & kTypeMask;
        res.AppendData(&original_nal_type, 1);
      }
      res.AppendData(payload.cdata() + kFuAHeaderSize,
                     payload.size() - kFuAHeaderSize);

      return res;
    }

    case kH264SingleNalu: {
      res.AppendData(kStartCode);
      res.AppendData(payload.cdata(), payload.size());
      return res;
    }
  }

  RTC_NOTREACHED();
  return res;
}

}  // namespace

H264PacketBuffer::H264PacketBuffer(bool idr_only_keyframes_allowed)
    : idr_only_keyframes_allowed_(idr_only_keyframes_allowed) {}

H264PacketBuffer::InsertResult H264PacketBuffer::InsertPacket(
    std::unique_ptr<Packet> packet) {
  InsertResult result;
  RTC_DCHECK(packet->video_header.codec == kVideoCodecH264);

  int64_t unwrapped_seq_num = seq_num_unwrapper_.Unwrap(packet->seq_num);
  ClearOldPackets(unwrapped_seq_num);

  const int index = EucMod(unwrapped_seq_num, kBufferSize);
  if (buffer_[index]) {
    // Duplicate, packet already in buffer.
    return result;
  }
  buffer_[index] = std::move(packet);

  result.packets = FindFrames(unwrapped_seq_num);
  last_marker_bit_unwrapped_ =
      result.packets.empty()
          ? last_marker_bit_unwrapped_
          : seq_num_unwrapper_.Unwrap(result.packets.back()->seq_num);

  return result;
}

const std::unique_ptr<H264PacketBuffer::Packet>& H264PacketBuffer::GetPacket(
    int64_t unwrapped_seq_num) const {
  return buffer_[EucMod(unwrapped_seq_num, kBufferSize)];
}

std::unique_ptr<H264PacketBuffer::Packet>& H264PacketBuffer::GetPacket(
    int64_t unwrapped_seq_num) {
  return buffer_[EucMod(unwrapped_seq_num, kBufferSize)];
}

bool H264PacketBuffer::BeginningOfStream(
    const H264PacketBuffer::Packet& packet) const {
  return HasSps(packet) ||
         (idr_only_keyframes_allowed_ && BeginningOfIdr(packet));
}

bool H264PacketBuffer::ContinuousWithLastMarkerBit(
    int64_t unwrapped_seq_num) const {
  return last_marker_bit_unwrapped_ == unwrapped_seq_num - 1;
}

bool H264PacketBuffer::ContinuousWithLastPacket(
    int64_t unwrapped_seq_num) const {
  const auto& prev_packet = GetPacket(unwrapped_seq_num - 1);

  if (prev_packet == nullptr) {
    return false;
  }
  if (prev_packet->continuous) {
    return true;
  }

  return false;
}

void H264PacketBuffer::ClearOldPackets(int64_t unwrapped_seq_num) {
  int64_t last_seq_num = last_seq_num_unwrapped_.value_or(unwrapped_seq_num);
  for (int64_t i = last_seq_num + 1; i <= unwrapped_seq_num; ++i) {
    GetPacket(i).reset();
  }

  if (last_seq_num_unwrapped_ < unwrapped_seq_num) {
    last_seq_num_unwrapped_ = unwrapped_seq_num;
  }
}

std::vector<std::unique_ptr<H264PacketBuffer::Packet>>
H264PacketBuffer::FindFrames(int64_t unwrapped_seq_num) {
  std::vector<std::unique_ptr<Packet>> found_frames;

  Packet* packet = GetPacket(unwrapped_seq_num).get();
  RTC_CHECK(packet != nullptr);

  if (ContinuousWithLastPacket(unwrapped_seq_num) ||
      ContinuousWithLastMarkerBit(unwrapped_seq_num) ||
      BeginningOfStream(*packet)) {
    packet->continuous = true;
  } else {
    return found_frames;
  }

  for (int64_t seq_num = unwrapped_seq_num;
       seq_num < unwrapped_seq_num + kBufferSize;) {
    // Last packet of the frame, try to assemble the frame.
    if (packet->marker_bit) {
      uint32_t rtp_timestamp = packet->timestamp;

      // Iterate backwards to find where the frame starts.
      for (int64_t seq_num_start = unwrapped_seq_num;
           seq_num_start > unwrapped_seq_num - kBufferSize; --seq_num_start) {
        auto& prev_packet = GetPacket(seq_num_start - 1);

        if (prev_packet == nullptr || prev_packet->timestamp != rtp_timestamp) {
          if (MaybeAssembleFrame(seq_num_start, seq_num, found_frames)) {
            // Frame was assembled, continue to look for more frames.
            break;
          } else {
            // Frame was not assembled, no subsequent frame will be continuous.
            return found_frames;
          }
        }
      }
    }

    seq_num++;
    packet = GetPacket(seq_num).get();
    if (packet == nullptr) {
      return found_frames;
    }

    packet->continuous = true;
  }

  return found_frames;
}

bool H264PacketBuffer::MaybeAssembleFrame(
    int64_t start_seq_num_unwrapped,
    int64_t end_sequence_number_unwrapped,
    std::vector<std::unique_ptr<Packet>>& frames) {
  bool has_sps = false;
  bool has_pps = false;
  bool has_idr = false;

  int width = -1;
  int height = -1;

  for (int64_t seq_num = start_seq_num_unwrapped;
       seq_num <= end_sequence_number_unwrapped; ++seq_num) {
    const auto& packet = GetPacket(seq_num);
    const auto& h264_header =
        absl::get<RTPVideoHeaderH264>(packet->video_header.video_type_header);
    for (size_t i = 0; i < h264_header.nalus_length; ++i) {
      has_idr |= h264_header.nalus[i].type == H264::NaluType::kIdr;
      has_sps |= h264_header.nalus[i].type == H264::NaluType::kSps;
      has_pps |= h264_header.nalus[i].type == H264::NaluType::kPps;
    }

    width = std::max<int>(packet->video_header.width, width);
    height = std::max<int>(packet->video_header.height, height);
  }

  if (has_idr) {
    if (!idr_only_keyframes_allowed_ && (!has_sps || !has_pps)) {
      return false;
    }
  }

  for (int64_t seq_num = start_seq_num_unwrapped;
       seq_num <= end_sequence_number_unwrapped; ++seq_num) {
    auto& packet = GetPacket(seq_num);

    packet->video_header.is_first_packet_in_frame =
        (seq_num == start_seq_num_unwrapped);
    packet->video_header.is_last_packet_in_frame =
        (seq_num == end_sequence_number_unwrapped);

    if (packet->video_header.is_first_packet_in_frame) {
      if (width > 0 && height > 0) {
        packet->video_header.width = width;
        packet->video_header.height = height;
      }

      packet->video_header.frame_type = has_idr
                                            ? VideoFrameType::kVideoFrameKey
                                            : VideoFrameType::kVideoFrameDelta;
    }

    packet->video_payload =
        FixVideoPayload(packet->video_payload, packet->video_header);

    frames.push_back(std::move(packet));
  }

  return true;
}

}  // namespace webrtc
