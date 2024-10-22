/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/h26x_packet_buffer.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/rtp_packet_info.h"
#include "api/video/video_frame_type.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/pps_parser.h"
#include "common_video/h264/sps_parser.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/h264_sprop_parameter_sets.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/sequence_number_util.h"
#ifdef RTC_ENABLE_H265
#include "common_video/h265/h265_common.h"
#endif

namespace webrtc {
namespace {

int64_t EuclideanMod(int64_t n, int64_t div) {
  RTC_DCHECK_GT(div, 0);
  return (n %= div) < 0 ? n + div : n;
}

bool IsFirstPacketOfFragment(const RTPVideoHeaderH264& h264_header) {
  return !h264_header.nalus.empty();
}

bool BeginningOfIdr(const H26xPacketBuffer::Packet& packet) {
  const auto& h264_header =
      absl::get<RTPVideoHeaderH264>(packet.video_header.video_type_header);
  const bool contains_idr_nalu =
      absl::c_any_of(h264_header.nalus, [](const auto& nalu_info) {
        return nalu_info.type == H264::NaluType::kIdr;
      });
  switch (h264_header.packetization_type) {
    case kH264StapA:
    case kH264SingleNalu: {
      return contains_idr_nalu;
    }
    case kH264FuA: {
      return contains_idr_nalu && IsFirstPacketOfFragment(h264_header);
    }
  }
}

bool HasSps(const H26xPacketBuffer::Packet& packet) {
  auto& h264_header =
      absl::get<RTPVideoHeaderH264>(packet.video_header.video_type_header);
  return absl::c_any_of(h264_header.nalus, [](const auto& nalu_info) {
    return nalu_info.type == H264::NaluType::kSps;
  });
}

int64_t* GetContinuousSequence(rtc::ArrayView<int64_t> last_continuous,
                               int64_t unwrapped_seq_num) {
  for (int64_t& last : last_continuous) {
    if (unwrapped_seq_num - 1 == last) {
      return &last;
    }
  }
  return nullptr;
}

#ifdef RTC_ENABLE_H265
bool HasVps(const H26xPacketBuffer::Packet& packet) {
  std::vector<H265::NaluIndex> nalu_indices =
      H265::FindNaluIndices(packet.video_payload);
  return absl::c_any_of((nalu_indices), [&packet](
                                            const H265::NaluIndex& nalu_index) {
    return H265::ParseNaluType(
               packet.video_payload.cdata()[nalu_index.payload_start_offset]) ==
           H265::NaluType::kVps;
  });
}
#endif

}  // namespace

H26xPacketBuffer::H26xPacketBuffer(bool h264_idr_only_keyframes_allowed)
    : h264_idr_only_keyframes_allowed_(h264_idr_only_keyframes_allowed) {
  last_continuous_in_sequence_.fill(std::numeric_limits<int64_t>::min());
}

H26xPacketBuffer::InsertResult H26xPacketBuffer::InsertPacket(
    std::unique_ptr<Packet> packet) {
  RTC_DCHECK(packet->video_header.codec == kVideoCodecH264 ||
             packet->video_header.codec == kVideoCodecH265);

  InsertResult result;

  int64_t unwrapped_seq_num = packet->sequence_number;
  auto& packet_slot = GetPacket(unwrapped_seq_num);
  if (packet_slot != nullptr &&
      AheadOrAt(packet_slot->timestamp, packet->timestamp)) {
    // The incoming `packet` is old or a duplicate.
    return result;
  } else {
    packet_slot = std::move(packet);
  }

  return FindFrames(unwrapped_seq_num);
}

std::unique_ptr<H26xPacketBuffer::Packet>& H26xPacketBuffer::GetPacket(
    int64_t unwrapped_seq_num) {
  return buffer_[EuclideanMod(unwrapped_seq_num, kBufferSize)];
}

bool H26xPacketBuffer::BeginningOfStream(
    const H26xPacketBuffer::Packet& packet) const {
  if (packet.codec() == kVideoCodecH264) {
    return HasSps(packet) ||
           (h264_idr_only_keyframes_allowed_ && BeginningOfIdr(packet));
#ifdef RTC_ENABLE_H265
  } else if (packet.codec() == kVideoCodecH265) {
    return HasVps(packet);
#endif
  }
  RTC_DCHECK_NOTREACHED();
  return false;
}

H26xPacketBuffer::InsertResult H26xPacketBuffer::FindFrames(
    int64_t unwrapped_seq_num) {
  InsertResult result;

  Packet* packet = GetPacket(unwrapped_seq_num).get();
  RTC_CHECK(packet != nullptr);

  // Check if the packet is continuous or the beginning of a new coded video
  // sequence.
  int64_t* last_continuous_unwrapped_seq_num =
      GetContinuousSequence(last_continuous_in_sequence_, unwrapped_seq_num);
  if (last_continuous_unwrapped_seq_num == nullptr) {
    if (!BeginningOfStream(*packet)) {
      return result;
    }

    last_continuous_in_sequence_[last_continuous_in_sequence_index_] =
        unwrapped_seq_num;
    last_continuous_unwrapped_seq_num =
        &last_continuous_in_sequence_[last_continuous_in_sequence_index_];
    last_continuous_in_sequence_index_ =
        (last_continuous_in_sequence_index_ + 1) %
        last_continuous_in_sequence_.size();
  }

  for (int64_t seq_num = unwrapped_seq_num;
       seq_num < unwrapped_seq_num + kBufferSize;) {
    RTC_DCHECK_GE(seq_num, *last_continuous_unwrapped_seq_num);

    // Packets that were never assembled into a completed frame will stay in
    // the 'buffer_'. Check that the `packet` sequence number match the expected
    // unwrapped sequence number.
    if (seq_num != packet->sequence_number) {
      return result;
    }

    *last_continuous_unwrapped_seq_num = seq_num;
    // Last packet of the frame, try to assemble the frame.
    if (packet->marker_bit) {
      uint32_t rtp_timestamp = packet->timestamp;

      // Iterate backwards to find where the frame starts.
      for (int64_t seq_num_start = seq_num;
           seq_num_start > seq_num - kBufferSize; --seq_num_start) {
        auto& prev_packet = GetPacket(seq_num_start - 1);

        if (prev_packet == nullptr || prev_packet->timestamp != rtp_timestamp) {
          if (MaybeAssembleFrame(seq_num_start, seq_num, result)) {
            // Frame was assembled, continue to look for more frames.
            break;
          } else {
            // Frame was not assembled, no subsequent frame will be continuous.
            return result;
          }
        }
      }
    }

    seq_num++;
    packet = GetPacket(seq_num).get();
    if (packet == nullptr) {
      return result;
    }
  }

  return result;
}

bool H26xPacketBuffer::MaybeAssembleFrame(int64_t start_seq_num_unwrapped,
                                          int64_t end_sequence_number_unwrapped,
                                          InsertResult& result) {
#ifdef RTC_ENABLE_H265
  bool has_vps = false;
#endif
  bool has_sps = false;
  bool has_pps = false;
  // Includes IDR, CRA and BLA for HEVC.
  bool has_idr = false;

  int width = -1;
  int height = -1;

  for (int64_t seq_num = start_seq_num_unwrapped;
       seq_num <= end_sequence_number_unwrapped; ++seq_num) {
    const auto& packet = GetPacket(seq_num);
    if (packet->codec() == kVideoCodecH264) {
      const auto& h264_header =
          absl::get<RTPVideoHeaderH264>(packet->video_header.video_type_header);
      for (const auto& nalu : h264_header.nalus) {
        has_idr |= nalu.type == H264::NaluType::kIdr;
        has_sps |= nalu.type == H264::NaluType::kSps;
        has_pps |= nalu.type == H264::NaluType::kPps;
      }
      if (has_idr) {
        if (!h264_idr_only_keyframes_allowed_ && (!has_sps || !has_pps)) {
          return false;
        }
      }
#ifdef RTC_ENABLE_H265
    } else if (packet->codec() == kVideoCodecH265) {
      std::vector<H265::NaluIndex> nalu_indices =
          H265::FindNaluIndices(packet->video_payload);
      for (const auto& nalu_index : nalu_indices) {
        uint8_t nalu_type = H265::ParseNaluType(
            packet->video_payload.cdata()[nalu_index.payload_start_offset]);
        has_idr |= (nalu_type >= H265::NaluType::kBlaWLp &&
                    nalu_type <= H265::NaluType::kRsvIrapVcl23);
        has_vps |= nalu_type == H265::NaluType::kVps;
        has_sps |= nalu_type == H265::NaluType::kSps;
        has_pps |= nalu_type == H265::NaluType::kPps;
      }
      if (has_idr) {
        if (!has_vps || !has_sps || !has_pps) {
          return false;
        }
      }
#endif  // RTC_ENABLE_H265
    }

    width = std::max<int>(packet->video_header.width, width);
    height = std::max<int>(packet->video_header.height, height);
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

    result.packets.push_back(std::move(packet));
  }

  return true;
}

}  // namespace webrtc
