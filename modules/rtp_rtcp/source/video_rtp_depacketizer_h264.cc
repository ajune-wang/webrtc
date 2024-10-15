/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/video_rtp_depacketizer_h264.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/types/variant.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/pps_parser.h"
#include "common_video/h264/sps_parser.h"
#include "common_video/h264/sps_vui_rewriter.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_format_h264.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "rtc_base/buffer.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr uint8_t start_code_h264[] = {0, 0, 0, 1};

constexpr size_t kNalHeaderSize = 1;
constexpr size_t kFuAHeaderSize = 2;

// The maximum expected growth from adding a VUI to the SPS. It's actually
// closer to 24 or so, but better safe than sorry.
constexpr size_t kMaxVuiSpsIncrease = 64;

struct FUAHeader {
  uint8_t fnri;
  uint8_t type;
  uint8_t original_nal_type;
  bool first_fragment;
};

std::optional<FUAHeader> ParseFUAHeader(rtc::ArrayView<const uint8_t> data) {
  if (data.size() < kFuAHeaderSize) {
    return std::nullopt;
  }
  FUAHeader header;
  header.fnri = data[0] & (kH264FBit | kH264NriMask);
  header.type = data[1] & kH264TypeMask;
  header.original_nal_type = data[1] & kH264TypeMask;
  header.first_fragment = (data[1] & kH264SBit) > 0;
  return header;
}

std::vector<rtc::ArrayView<const uint8_t>> ParseStapA(
    rtc::ArrayView<const uint8_t> data) {
  std::vector<rtc::ArrayView<const uint8_t>> nal_units;
  rtc::ByteBufferReader reader(data);
  if (!reader.Consume(kNalHeaderSize)) {
    return nal_units;
  }

  while (reader.Length() > 0) {
    uint16_t nalu_size;
    if (!reader.ReadUInt16(&nalu_size)) {
      return {};
    }
    if (nalu_size == 0 || nalu_size > reader.Length()) {
      return {};
    }
    nal_units.emplace_back(reader.Data(), nalu_size);
    reader.Consume(nalu_size);
  }
  return nal_units;
}

std::optional<VideoRtpDepacketizer::ParsedRtpPayload> ProcessStapAOrSingleNalu(
    rtc::CopyOnWriteBuffer rtp_payload) {
  rtc::ArrayView<const uint8_t> payload_data(rtp_payload);
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload(
      std::in_place);
  parsed_payload->video_payload = rtp_payload;
  parsed_payload->video_header.width = 0;
  parsed_payload->video_header.height = 0;
  parsed_payload->video_header.codec = kVideoCodecH264;
  parsed_payload->video_header.simulcastIdx = 0;
  parsed_payload->video_header.is_first_packet_in_frame = false;
  auto& h264_header = parsed_payload->video_header.video_type_header
                          .emplace<RTPVideoHeaderH264>();

  uint8_t nal_type = payload_data[0] & kH264TypeMask;
  std::vector<rtc::ArrayView<const uint8_t>> nal_units;
  if (nal_type == H264::NaluType::kStapA) {
    nal_units = ParseStapA(payload_data);
    if (nal_units.empty()) {
      RTC_LOG(LS_ERROR) << "Incorrect StapA packet.";
      return std::nullopt;
    }
    h264_header.packetization_type = kH264StapA;
    h264_header.nalu_type = nal_units[0][0] & kH264TypeMask;
  } else {
    h264_header.packetization_type = kH264SingleNalu;
    h264_header.nalu_type = nal_type;
    nal_units.push_back(payload_data);
  }

  parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameDelta;

  for (const rtc::ArrayView<const uint8_t>& nal_unit : nal_units) {
    NaluInfo nalu;
    nalu.type = nal_unit[0] & kH264TypeMask;
    nalu.sps_id = -1;
    nalu.pps_id = -1;
    rtc::ArrayView<const uint8_t> nalu_data =
        nal_unit.subview(H264::kNaluTypeSize);

    if (nalu_data.empty()) {
      RTC_LOG(LS_ERROR) << "Empty NAL unit found.";
      return std::nullopt;
    }

    switch (nalu.type) {
      case H264::NaluType::kSps: {
        std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(nalu_data);
        if (sps) {
          nalu.sps_id = sps->id;
          parsed_payload->video_header.width = sps->width;
          parsed_payload->video_header.height = sps->height;
          parsed_payload->video_header.frame_type =
              VideoFrameType::kVideoFrameKey;
          parsed_payload->video_header.is_first_packet_in_frame = true;
        } else {
          RTC_LOG(LS_WARNING) << "Failed to parse SPS NAL unit.";
          return std::nullopt;
        }
        break;
      }
      case H264::NaluType::kPps: {
        uint32_t pps_id;
        uint32_t sps_id;
        if (PpsParser::ParsePpsIds(nalu_data, &pps_id, &sps_id)) {
          nalu.pps_id = pps_id;
          nalu.sps_id = sps_id;
        } else {
          RTC_LOG(LS_WARNING)
              << "Failed to parse PPS id and SPS id from PPS slice.";
          return std::nullopt;
        }
        parsed_payload->video_header.is_first_packet_in_frame = true;
        break;
      }
      case H264::NaluType::kIdr:
        parsed_payload->video_header.frame_type =
            VideoFrameType::kVideoFrameKey;
        [[fallthrough]];
      case H264::NaluType::kSlice: {
        std::optional<PpsParser::SliceHeader> slice_header =
            PpsParser::ParseSliceHeader(nalu_data);
        if (slice_header) {
          nalu.pps_id = slice_header->pic_parameter_set_id;
          if (slice_header->first_mb_in_slice == 0) {
            parsed_payload->video_header.is_first_packet_in_frame = true;
          }
        } else {
          RTC_LOG(LS_WARNING) << "Failed to parse header from slice of type: "
                              << static_cast<int>(nalu.type);
          return std::nullopt;
        }
        break;
      }
      case H264::NaluType::kAud:
        parsed_payload->video_header.is_first_packet_in_frame = true;
        break;
      case H264::NaluType::kSei:
        parsed_payload->video_header.is_first_packet_in_frame = true;
        break;
      // Slices below don't contain SPS or PPS ids.
      case H264::NaluType::kEndOfSequence:
      case H264::NaluType::kEndOfStream:
      case H264::NaluType::kFiller:
      case H264::NaluType::kStapA:
      case H264::NaluType::kFuA:
        RTC_LOG(LS_WARNING) << "Unexpected STAP-A or FU-A received.";
        return std::nullopt;
    }

    h264_header.nalus.push_back(nalu);
  }

  return parsed_payload;
}

std::optional<VideoRtpDepacketizer::ParsedRtpPayload> ParseFuaNalu(
    rtc::CopyOnWriteBuffer rtp_payload) {
  std::optional<FUAHeader> fua_header = ParseFUAHeader(rtp_payload);
  if (!fua_header) {
    RTC_LOG(LS_ERROR) << "FU-A NAL units truncated.";
    return std::nullopt;
  }
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload(
      std::in_place);
  bool is_first_packet_in_frame = false;
  NaluInfo nalu;
  nalu.type = fua_header->original_nal_type;
  nalu.sps_id = -1;
  nalu.pps_id = -1;
  if (fua_header->first_fragment) {
    if (fua_header->original_nal_type == H264::NaluType::kIdr ||
        fua_header->original_nal_type == H264::NaluType::kSlice) {
      std::optional<PpsParser::SliceHeader> slice_header =
          PpsParser::ParseSliceHeader(rtc::ArrayView<const uint8_t>(rtp_payload)
                                          .subview(kFuAHeaderSize));
      if (slice_header) {
        nalu.pps_id = slice_header->pic_parameter_set_id;
        is_first_packet_in_frame = slice_header->first_mb_in_slice == 0;
      } else {
        RTC_LOG(LS_WARNING)
            << "Failed to parse PPS from first fragment of FU-A NAL "
               "unit with original type: "
            << static_cast<int>(nalu.type);
        return std::nullopt;
      }
    }
  }

  parsed_payload->video_payload = rtp_payload;
  parsed_payload->video_header.width = 0;
  parsed_payload->video_header.height = 0;
  parsed_payload->video_header.codec = kVideoCodecH264;
  parsed_payload->video_header.simulcastIdx = 0;
  parsed_payload->video_header.is_first_packet_in_frame =
      is_first_packet_in_frame;
  auto& h264_header = parsed_payload->video_header.video_type_header
                          .emplace<RTPVideoHeaderH264>();
  h264_header.packetization_type = kH264FuA;
  h264_header.nalu_type = fua_header->original_nal_type;
  if (fua_header->first_fragment) {
    h264_header.nalus = {nalu};
  }
  return parsed_payload;
}

}  // namespace

std::optional<VideoRtpDepacketizer::ParsedRtpPayload>
VideoRtpDepacketizerH264::Parse(rtc::CopyOnWriteBuffer rtp_payload) {
  if (rtp_payload.size() == 0) {
    RTC_LOG(LS_ERROR) << "Empty payload.";
    return std::nullopt;
  }

  uint8_t nal_type = rtp_payload.cdata()[0] & kH264TypeMask;

  if (nal_type == H264::NaluType::kFuA) {
    // Fragmented NAL units (FU-A).
    return ParseFuaNalu(std::move(rtp_payload));
  } else {
    // We handle STAP-A and single NALU's the same way here. The jitter buffer
    // will depacketize the STAP-A into NAL units later.
    return ProcessStapAOrSingleNalu(std::move(rtp_payload));
  }
}

rtc::scoped_refptr<EncodedImageBuffer> VideoRtpDepacketizerH264::AssembleFrame(
    rtc::ArrayView<const rtc::ArrayView<const uint8_t>> rtp_payloads) {
  size_t frame_size = 0;

  std::vector<rtc::ArrayView<const uint8_t>> nalus;
  // Calculate approx frame size to avoid doing extra reallocs and collect NAL
  // units excluding STAP-As.
  for (rtc::ArrayView<const uint8_t> rtp_payload : rtp_payloads) {
    uint8_t nal_type = rtp_payload[0] & kH264TypeMask;

    switch (nal_type) {
      case H264::NaluType::kFuA: {
        std::optional<FUAHeader> fua_header = ParseFUAHeader(rtp_payload);
        if (!fua_header) {
          RTC_LOG(LS_ERROR) << "FU-A NAL units truncated.";
          return nullptr;
        }
        if (fua_header->first_fragment) {
          frame_size += sizeof(start_code_h264) + kNalHeaderSize;
        }
        frame_size += rtp_payload.size() - kFuAHeaderSize;
        nalus.push_back(rtp_payload);
        break;
      }
      case H264::NaluType::kStapA: {
        std::vector<rtc::ArrayView<const uint8_t>> stapa_nals =
            ParseStapA(rtp_payload);
        if (stapa_nals.empty()) {
          RTC_LOG(LS_ERROR)
              << "StapA packet with incorrect NALU packet lengths.";
          return nullptr;
        }

        for (rtc::ArrayView<const uint8_t> stapa_nal : stapa_nals) {
          if ((stapa_nal[0] & kH264TypeMask) == H264::NaluType::kSps) {
            // Allocate enought size for SPS rewritting.
            frame_size += sizeof(start_code_h264) + rtp_payload.size() +
                          kMaxVuiSpsIncrease;
          } else {
            frame_size += sizeof(start_code_h264) + rtp_payload.size();
          }
          nalus.push_back(rtp_payload);
        }
        break;
      }
      case H264::NaluType::kSps:
        // Allocate enought size for SPS rewritting.
        frame_size +=
            sizeof(start_code_h264) + rtp_payload.size() + kMaxVuiSpsIncrease;
        nalus.push_back(rtp_payload);
        break;
      default:
        frame_size += sizeof(start_code_h264) + rtp_payload.size();
        nalus.push_back(rtp_payload);
    }
  }

  if (has_out_of_band_sps_pps_) {
    for (auto& [sps_id, sps] : sps_data_) {
      if (!sps.data.empty()) {
        frame_size += sizeof(start_code_h264) + sps.data.size();
      }
    }
    for (auto& [sps_id, pps] : pps_data_) {
      if (!pps.data.empty()) {
        frame_size += sizeof(start_code_h264) + pps.data.size();
      }
    }
  }

  // Reserve enought size for full packetized frame.
  rtc::ByteBufferWriter writer(nullptr, frame_size);

  for (rtc::ArrayView<const uint8_t> nal : nalus) {
    uint8_t nal_type = nal[0] & kH264TypeMask;
    switch (nal_type) {
      case H264::NaluType::kFuA: {
        std::optional<FUAHeader> fua_header = ParseFUAHeader(nal);
        if (!fua_header) {
          RTC_LOG(LS_ERROR) << "FU-A NAL units truncated.";
          return nullptr;
        }

        rtc::ArrayView<const uint8_t> nal_data = nal.subview(kFuAHeaderSize);

        if (fua_header->first_fragment) {
          // Check if we need to insert out of band data.
          if (has_out_of_band_sps_pps_ &&
              fua_header->original_nal_type == H264::NaluType::kIdr) {
            std::optional<PpsParser::SliceHeader> slice_header =
                PpsParser::ParseSliceHeader(nal_data);
            if (slice_header && slice_header->first_mb_in_slice == 0) {
              auto pps = pps_data_.find(slice_header->pic_parameter_set_id);
              if (pps == pps_data_.end()) {
                RTC_LOG(LS_WARNING)
                    << "No PPS with id << "
                    << slice_header->pic_parameter_set_id << " received";
                return nullptr;
              }
              auto sps = sps_data_.find(pps->second.sps_id);
              if (sps == sps_data_.end()) {
                RTC_LOG(LS_WARNING) << "No SPS with id << "
                                    << pps->second.sps_id << " received";
                return nullptr;
              }
              // Add outband SPS and PPS.
              if (!sps->second.data.empty() && !pps->second.data.empty()) {
                writer.Write(start_code_h264);
                writer.Write(sps->second.data);
                writer.Write(start_code_h264);
                writer.Write(pps->second.data);
              }
            }

            // Reconstruct NAL header.
            uint8_t original_nal_header =
                fua_header->fnri | fua_header->original_nal_type;
            writer.Write(start_code_h264);
            writer.WriteUInt8(original_nal_header);
          }
        }
        writer.Write(nal_data);
        break;
      }
      case H264::NaluType::kSps: {
        // Check if VUI is present in SPS and if it needs to be modified to
        // avoid excessive decoder latency.
        rtc::Buffer output_buffer;

        std::optional<SpsParser::SpsState> sps;

        SpsVuiRewriter::ParseResult result = SpsVuiRewriter::ParseAndRewriteSps(
            nal.subview(kNalHeaderSize), &sps, nullptr, &output_buffer,
            SpsVuiRewriter::Direction::kIncoming);
        switch (result) {
          case SpsVuiRewriter::ParseResult::kFailure:
            RTC_LOG(LS_WARNING) << "Failed to parse SPS NAL unit.";
            return nullptr;
          case SpsVuiRewriter::ParseResult::kVuiRewritten:
            // Append modified packet.
            writer.Write(start_code_h264);
            writer.Write(output_buffer);
            break;
          case SpsVuiRewriter::ParseResult::kVuiOk:
            // Append modified packet.
            writer.Write(start_code_h264);
            writer.Write(nal);
            break;
        }
        sps_data_.try_emplace(sps->id);

        break;
      }
      case H264::NaluType::kPps: {
        uint32_t pps_id;
        uint32_t sps_id;
        if (PpsParser::ParsePpsIds(nal.subview(kNalHeaderSize), &pps_id,
                                   &sps_id)) {
          pps_data_[pps_id].sps_id = sps_id;
        } else {
          RTC_LOG(LS_ERROR) << "Failed to parse PPS.";
          return nullptr;
        }
        writer.Write(start_code_h264);
        writer.Write(nal);
        break;
      }
      case H264::NaluType::kIdr: {
        // Check if we need to insert out of band data.
        if (has_out_of_band_sps_pps_) {
          std::optional<PpsParser::SliceHeader> slice_header =
              PpsParser::ParseSliceHeader(nal.subview(kNalHeaderSize));
          if (slice_header && slice_header->first_mb_in_slice == 0) {
            auto pps = pps_data_.find(slice_header->pic_parameter_set_id);
            if (pps == pps_data_.end()) {
              RTC_LOG(LS_WARNING)
                  << "No PPS with id << " << slice_header->pic_parameter_set_id
                  << " received";
              return nullptr;
            }
            auto sps = sps_data_.find(pps->second.sps_id);
            if (sps == sps_data_.end()) {
              RTC_LOG(LS_WARNING)
                  << "No SPS with id << " << pps->second.sps_id << " received";
              return nullptr;
            }
            // Add outband SPS and PPS.
            if (!sps->second.data.empty() && !pps->second.data.empty()) {
              writer.Write(start_code_h264);
              writer.Write(sps->second.data);
              writer.Write(start_code_h264);
              writer.Write(pps->second.data);
            }
          }
        }
        writer.Write(start_code_h264);
        writer.Write(nal);
        break;
      }
      default:
        writer.Write(start_code_h264);
        writer.Write(nal);
    }
  }
  return webrtc::EncodedImageBuffer::Create(std::move(writer).Extract());
}

void VideoRtpDepacketizerH264::InsertSpsPpsNalus(
    const std::vector<uint8_t>& sps,
    const std::vector<uint8_t>& pps) {
  constexpr size_t kNaluHeaderOffset = 1;
  if (sps.size() < kNaluHeaderOffset) {
    RTC_LOG(LS_WARNING) << "SPS size  " << sps.size() << " is smaller than "
                        << kNaluHeaderOffset;
    return;
  }
  if ((sps[0] & 0x1f) != H264::NaluType::kSps) {
    RTC_LOG(LS_WARNING) << "SPS Nalu header missing";
    return;
  }
  if (pps.size() < kNaluHeaderOffset) {
    RTC_LOG(LS_WARNING) << "PPS size  " << pps.size() << " is smaller than "
                        << kNaluHeaderOffset;
    return;
  }
  if ((pps[0] & 0x1f) != H264::NaluType::kPps) {
    RTC_LOG(LS_WARNING) << "SPS Nalu header missing";
    return;
  }
  std::optional<SpsParser::SpsState> parsed_sps = SpsParser::ParseSps(
      rtc::ArrayView<const uint8_t>(sps).subview(kNaluHeaderOffset));
  std::optional<PpsParser::PpsState> parsed_pps = PpsParser::ParsePps(
      rtc::ArrayView<const uint8_t>(pps).subview(kNaluHeaderOffset));

  if (!parsed_sps) {
    RTC_LOG(LS_WARNING) << "Failed to parse SPS.";
  }

  if (!parsed_pps) {
    RTC_LOG(LS_WARNING) << "Failed to parse PPS.";
  }

  if (!parsed_pps || !parsed_sps) {
    return;
  }

  SpsInfo sps_info;

  sps_info.data.SetData(sps);
  sps_data_[parsed_sps->id] = std::move(sps_info);

  PpsInfo pps_info;
  pps_info.sps_id = parsed_pps->sps_id;
  pps_info.data.SetData(pps);
  pps_data_[parsed_pps->id] = std::move(pps_info);

  has_out_of_band_sps_pps_ = true;

  RTC_LOG(LS_INFO) << "Inserted SPS id " << parsed_sps->id << " and PPS id "
                   << parsed_pps->id << " (referencing SPS "
                   << parsed_pps->sps_id << ")";
}

}  // namespace webrtc
