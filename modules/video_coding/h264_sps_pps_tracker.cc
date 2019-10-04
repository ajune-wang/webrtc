/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/h264_sps_pps_tracker.h"

#include <string>
#include <utility>

#include "common_video/h264/h264_common.h"
#include "common_video/h264/pps_parser.h"
#include "common_video/h264/sps_parser.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/packet_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace video_coding {

namespace {
constexpr size_t kNaluHeaderSize = 1;
const uint8_t start_code_h264[] = {0, 0, 0, 1};
}  // namespace

H264SpsPpsTracker::PpsInfo::PpsInfo() = default;
H264SpsPpsTracker::PpsInfo::PpsInfo(PpsInfo&& rhs) = default;
H264SpsPpsTracker::PpsInfo& H264SpsPpsTracker::PpsInfo::operator=(
    PpsInfo&& rhs) = default;
H264SpsPpsTracker::PpsInfo::~PpsInfo() = default;

H264SpsPpsTracker::SpsInfo::SpsInfo() = default;
H264SpsPpsTracker::SpsInfo::SpsInfo(SpsInfo&& rhs) = default;
H264SpsPpsTracker::SpsInfo& H264SpsPpsTracker::SpsInfo::operator=(
    SpsInfo&& rhs) = default;
H264SpsPpsTracker::SpsInfo::~SpsInfo() = default;

H264SpsPpsTracker::H264SpsPpsTracker()
    : insert_inband_sps_pps_before_idr_(
          !field_trial::IsDisabled("WebRTC-InsertInBandSpsPps")),
      reset_end_of_frame_flag_on_nonvlc_packet_(!field_trial::IsDisabled(
          "WebRTC-ResetEndOfFrameFlagOnNonVlcPacket")) {}
H264SpsPpsTracker::~H264SpsPpsTracker() = default;

static bool HasVclData(const VCMPacket& packet) {
  const auto* h264_header =
      absl::get_if<RTPVideoHeaderH264>(&packet.video_header.video_type_header);
  if (h264_header->nalus_length == 0) {
    return h264_header->nalu_type == H264::NaluType::kIdr ||
           h264_header->nalu_type == H264::NaluType::kSlice;
  }

  for (size_t i = 0; i < h264_header->nalus_length; ++i) {
    if (h264_header->nalus[i].type == H264::NaluType::kIdr ||
        h264_header->nalus[i].type == H264::NaluType::kSlice) {
      return true;
    }
  }

  return false;
}

H264SpsPpsTracker::PacketAction H264SpsPpsTracker::CopyAndFixBitstream(
    VCMPacket* packet) {
  RTC_DCHECK(packet->codec() == kVideoCodecH264);

  const uint8_t* data = packet->dataPtr;
  const size_t data_size = packet->sizeBytes;
  const RTPVideoHeader& video_header = packet->video_header;
  auto& h264_header =
      absl::get<RTPVideoHeaderH264>(packet->video_header.video_type_header);

  bool append_sps_pps = false;
  auto sps = sps_data_.end();
  auto pps = pps_data_.end();

  if (insert_inband_sps_pps_before_idr_) {
    StoreInBandSpsPps(*packet);
  }

  for (size_t i = 0; i < h264_header.nalus_length; ++i) {
    const NaluInfo& nalu = h264_header.nalus[i];
    switch (nalu.type) {
      case H264::NaluType::kSps: {
        sps_data_[nalu.sps_id].width = packet->width();
        sps_data_[nalu.sps_id].height = packet->height();
        break;
      }
      case H264::NaluType::kPps: {
        pps_data_[nalu.pps_id].sps_id = nalu.sps_id;
        break;
      }
      case H264::NaluType::kIdr: {
        // If this is the first packet of an IDR, make sure we have the required
        // SPS/PPS and also calculate how much extra space we need in the buffer
        // to prepend the SPS/PPS to the bitstream with start codes.
        if (video_header.is_first_packet_in_frame) {
          if (nalu.pps_id == -1) {
            RTC_LOG(LS_WARNING) << "No PPS id in IDR nalu.";
            return kRequestKeyframe;
          }

          pps = pps_data_.find(nalu.pps_id);
          if (pps == pps_data_.end()) {
            RTC_LOG(LS_WARNING)
                << "No PPS with id << " << nalu.pps_id << " received";
            return kRequestKeyframe;
          }

          sps = sps_data_.find(pps->second.sps_id);
          if (sps == sps_data_.end()) {
            RTC_LOG(LS_WARNING)
                << "No SPS with id << " << pps->second.sps_id << " received";
            return kRequestKeyframe;
          }

          // Since the first packet of every keyframe should have its width and
          // height set we set it here in the case of it being supplied out of
          // band.
          packet->video_header.width = sps->second.width;
          packet->video_header.height = sps->second.height;

          if (!sps->second.data.empty() && !pps->second.data.empty()) {
            RTC_DCHECK_GT(sps->second.data.size(), 0);
            RTC_DCHECK_GT(pps->second.data.size(), 0);
            // Out-of-band headers do not have RTP sequence number.
            const bool out_of_band_sps_pps =
                !sps->second.rtp_seq_num.has_value() &&
                !pps->second.rtp_seq_num.has_value();
            if (out_of_band_sps_pps) {
              append_sps_pps = true;
            } else if (insert_inband_sps_pps_before_idr_ &&
                       !IsSameTimestamp(sps->second, pps->second, *packet) &&
                       IsContinuousSeqNum(sps->second, pps->second, *packet)) {
              // Insert out-of-band SPS/PPS or in-band headers if thier
              // timestamp is different from IDR's one and there is no gap in
              // sequence number.
              append_sps_pps = true;
            }
          }
        }
        break;
      }
      default:
        break;
    }
  }

  RTC_CHECK(!append_sps_pps ||
            (sps != sps_data_.end() && pps != pps_data_.end()));

  // Calculate how much space we need for the rest of the bitstream.
  size_t required_size = 0;

  if (append_sps_pps) {
    required_size += sps->second.data.size() + sizeof(start_code_h264);
    required_size += pps->second.data.size() + sizeof(start_code_h264);
  }

  if (h264_header.packetization_type == kH264StapA) {
    const uint8_t* nalu_ptr = data + 1;
    while (nalu_ptr < data + data_size) {
      RTC_DCHECK(video_header.is_first_packet_in_frame);
      required_size += sizeof(start_code_h264);

      // The first two bytes describe the length of a segment.
      uint16_t segment_length = nalu_ptr[0] << 8 | nalu_ptr[1];
      nalu_ptr += 2;

      required_size += segment_length;
      nalu_ptr += segment_length;
    }
  } else {
    if (h264_header.nalus_length > 0) {
      required_size += sizeof(start_code_h264);
    }
    required_size += data_size;
  }

  // Then we copy to the new buffer.
  uint8_t* buffer = new uint8_t[required_size];
  uint8_t* insert_at = buffer;

  if (append_sps_pps) {
    // Insert SPS.
    memcpy(insert_at, start_code_h264, sizeof(start_code_h264));
    insert_at += sizeof(start_code_h264);
    memcpy(insert_at, sps->second.data.data(), sps->second.data.size());
    insert_at += sps->second.data.size();

    // Insert PPS.
    memcpy(insert_at, start_code_h264, sizeof(start_code_h264));
    insert_at += sizeof(start_code_h264);
    memcpy(insert_at, pps->second.data.data(), pps->second.data.size());
    insert_at += pps->second.data.size();

    // Update codec header to reflect the newly added SPS and PPS.
    NaluInfo sps_info;
    sps_info.type = H264::NaluType::kSps;
    sps_info.sps_id = sps->first;
    sps_info.pps_id = -1;
    NaluInfo pps_info;
    pps_info.type = H264::NaluType::kPps;
    pps_info.sps_id = sps->first;
    pps_info.pps_id = pps->first;
    if (h264_header.nalus_length + 2 <= kMaxNalusPerPacket) {
      h264_header.nalus[h264_header.nalus_length++] = sps_info;
      h264_header.nalus[h264_header.nalus_length++] = pps_info;
    } else {
      RTC_LOG(LS_WARNING) << "Not enough space in H.264 codec header to insert "
                             "SPS/PPS provided out-of-band.";
    }
  }

  // Copy the rest of the bitstream and insert start codes.
  if (h264_header.packetization_type == kH264StapA) {
    const uint8_t* nalu_ptr = data + 1;
    while (nalu_ptr < data + data_size) {
      memcpy(insert_at, start_code_h264, sizeof(start_code_h264));
      insert_at += sizeof(start_code_h264);

      // The first two bytes describe the length of a segment.
      uint16_t segment_length = nalu_ptr[0] << 8 | nalu_ptr[1];
      nalu_ptr += 2;

      size_t copy_end = nalu_ptr - data + segment_length;
      if (copy_end > data_size) {
        delete[] buffer;
        return kDrop;
      }

      memcpy(insert_at, nalu_ptr, segment_length);
      insert_at += segment_length;
      nalu_ptr += segment_length;
    }
  } else {
    if (h264_header.nalus_length > 0) {
      memcpy(insert_at, start_code_h264, sizeof(start_code_h264));
      insert_at += sizeof(start_code_h264);
    }
    memcpy(insert_at, data, data_size);
  }

  packet->dataPtr = buffer;
  packet->sizeBytes = required_size;

  // If packet does not contain VCL NAL unit(s) then reset frame end-of-frame
  // flag to prevent this packet to be interpreted as a frame by the packet
  // buffer.
  if (packet->is_last_packet_in_frame() && !HasVclData(*packet)) {
    packet->video_header.is_last_packet_in_frame = false;
  }

  return kInsert;
}

void H264SpsPpsTracker::InsertSpsPpsNalus(const std::vector<uint8_t>& sps,
                                          const std::vector<uint8_t>& pps) {
  if (sps.size() < kNaluHeaderSize) {
    RTC_LOG(LS_WARNING) << "SPS size  " << sps.size() << " is smaller than "
                        << kNaluHeaderSize;
    return;
  }
  if ((sps[0] & 0x1f) != H264::NaluType::kSps) {
    RTC_LOG(LS_WARNING) << "SPS Nalu header missing";
    return;
  }
  if (pps.size() < kNaluHeaderSize) {
    RTC_LOG(LS_WARNING) << "PPS size  " << pps.size() << " is smaller than "
                        << kNaluHeaderSize;
    return;
  }
  if ((pps[0] & 0x1f) != H264::NaluType::kPps) {
    RTC_LOG(LS_WARNING) << "SPS Nalu header missing";
    return;
  }

  absl::optional<SpsParser::SpsState> parsed_sps = SpsParser::ParseSps(
      sps.data() + kNaluHeaderSize, sps.size() - kNaluHeaderSize);
  absl::optional<PpsParser::PpsState> parsed_pps = PpsParser::ParsePps(
      pps.data() + kNaluHeaderSize, pps.size() - kNaluHeaderSize);
  if (!parsed_sps) {
    RTC_LOG(LS_WARNING) << "Failed to parse SPS.";
  }
  if (!parsed_pps) {
    RTC_LOG(LS_WARNING) << "Failed to parse PPS.";
  }
  if (!parsed_pps || !parsed_sps) {
    return;
  }

  StoreSps(sps.data(), sps.size(), parsed_sps->id, parsed_sps->width,
           parsed_sps->height, absl::nullopt, absl::nullopt);

  StorePps(pps.data(), pps.size(), parsed_sps->id, parsed_pps->id,
           absl::nullopt, absl::nullopt);
}

void H264SpsPpsTracker::StoreInBandSpsPps(const VCMPacket& packet) {
  auto& h264_header =
      absl::get<RTPVideoHeaderH264>(packet.video_header.video_type_header);

  if (h264_header.packetization_type == kH264SingleNalu) {
    RTC_DCHECK_EQ(h264_header.nalus_length, 1);
    if (h264_header.nalus[0].type == H264::NaluType::kSps) {
      StoreSps(packet.dataPtr, packet.sizeBytes, h264_header.nalus[0].sps_id,
               packet.width(), packet.height(), packet.seqNum,
               packet.timestamp);
    } else if (h264_header.nalus[0].type == H264::NaluType::kPps) {
      StorePps(packet.dataPtr, packet.sizeBytes, h264_header.nalus[0].sps_id,
               h264_header.nalus[0].pps_id, packet.seqNum, packet.timestamp);
    }
  } else if (h264_header.packetization_type == kH264StapA) {
    // Find SPS/PPS in packet buffer.
    const uint8_t* data_cur = packet.dataPtr + kNaluHeaderSize;
    const uint8_t* data_end = packet.dataPtr + packet.sizeBytes;
    for (size_t nalu_num = 0; nalu_num < h264_header.nalus_length; ++nalu_num) {
      const NaluInfo& nalu_info = h264_header.nalus[nalu_num];
      const int nalu_size_bytes = data_cur[0] << 8 | data_cur[1];
      data_cur += 2;
      if (data_cur + nalu_size_bytes > data_end) {
        RTC_LOG(LS_WARNING) << "Failed to find a NALU in packet.";
        return;
      }

      RTC_DCHECK_EQ(nalu_info.type, (*data_cur) & 0x1f);

      if (nalu_info.type == H264::NaluType::kSps) {
        StoreSps(data_cur, nalu_size_bytes, nalu_info.sps_id, packet.width(),
                 packet.height(), packet.seqNum, packet.timestamp);
      } else if (nalu_info.type == H264::NaluType::kPps) {
        StorePps(data_cur, nalu_size_bytes, nalu_info.sps_id, nalu_info.pps_id,
                 packet.seqNum, packet.timestamp);
      }

      data_cur += nalu_size_bytes;
    }
  }
}

void H264SpsPpsTracker::StoreSps(const uint8_t* data,
                                 size_t data_size_bytes,
                                 uint32_t sps_id,
                                 uint32_t width,
                                 uint32_t height,
                                 absl::optional<uint16_t> rtp_seq_num,
                                 absl::optional<uint32_t> rtp_timestamp) {
  SpsInfo sps_info;
  sps_info.width = width;
  sps_info.height = height;
  sps_info.rtp_seq_num = rtp_seq_num;
  sps_info.rtp_timestamp = rtp_timestamp;
  sps_info.data.assign(data, data + data_size_bytes);
  sps_data_[sps_id] = std::move(sps_info);
}

void H264SpsPpsTracker::StorePps(const uint8_t* data,
                                 size_t data_size_bytes,
                                 uint32_t sps_id,
                                 uint32_t pps_id,
                                 absl::optional<uint16_t> rtp_seq_num,
                                 absl::optional<uint32_t> rtp_timestamp) {
  PpsInfo pps_info;
  pps_info.sps_id = sps_id;
  pps_info.rtp_seq_num = rtp_seq_num;
  pps_info.rtp_timestamp = rtp_timestamp;
  pps_info.data.assign(data, data + data_size_bytes);
  pps_data_[pps_id] = std::move(pps_info);
}

bool H264SpsPpsTracker::IsContinuousSeqNum(const SpsInfo& sps,
                                           const PpsInfo& pps,
                                           const VCMPacket& packet) {
  RTC_DCHECK(sps.rtp_seq_num.has_value());
  RTC_DCHECK(pps.rtp_seq_num.has_value());
  const uint16_t seq_num = packet.seqNum;
  return pps.rtp_seq_num == (uint16_t)(seq_num - 1) &&
         (sps.rtp_seq_num == (uint16_t)(seq_num - 1) ||
          sps.rtp_seq_num == (uint16_t)(seq_num - 2));
}

bool H264SpsPpsTracker::IsSameTimestamp(const SpsInfo& sps,
                                        const PpsInfo& pps,
                                        const VCMPacket& packet) {
  RTC_DCHECK(sps.rtp_timestamp.has_value());
  RTC_DCHECK(pps.rtp_timestamp.has_value());
  return packet.timestamp == sps.rtp_timestamp &&
         packet.timestamp == pps.rtp_timestamp;
}

}  // namespace video_coding
}  // namespace webrtc
