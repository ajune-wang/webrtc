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
#include "rtc_base/byte_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

const uint8_t start_code_h264[] = {0, 0, 0, 1};

constexpr size_t kNalHeaderSize = 1;
constexpr size_t kFuAHeaderSize = 2;
constexpr size_t kLengthFieldSize = 2;
constexpr size_t kStapAHeaderSize = kNalHeaderSize + kLengthFieldSize;

bool ParseStapA(rtc::ArrayView<const uint8_t> data,
                std::vector<rtc::ArrayView<const uint8_t>>* nalus) {
  // Skip the StapA header (StapA NAL type + length).
  if (data.size() <= kStapAHeaderSize) {
    RTC_LOG(LS_ERROR) << "StapA header truncated.";
    return false;
  }
  rtc::ByteBufferReader reader(data);
  reader.Consume(kStapAHeaderSize);

  while (reader.Length() > 0) {
    uint16_t nalu_size;
    // Buffer doesn't contain room for additional nalu length.
    if (reader.ReadUInt16(&nalu_size))
      return false;

    if (nalu_size > reader.Length())
      return false;

    nalus->push_back({reader.Data(), nalu_size});

    reader.Consume(nalu_size);
  }
  return true;
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
  parsed_payload->video_header.is_first_packet_in_frame = true;
  auto& h264_header = parsed_payload->video_header.video_type_header
                          .emplace<RTPVideoHeaderH264>();

  uint8_t nal_type = payload_data[0] & kH264TypeMask;
  std::vector<rtc::ArrayView<const uint8_t>> nalus;
  if (nal_type == H264::NaluType::kStapA) {
    if (!ParseStapA(payload_data, &nalus)) {
      RTC_LOG(LS_ERROR) << "StapA packet with incorrect NALU packet lengths.";
      return std::nullopt;
    }
    h264_header.packetization_type = kH264StapA;
  } else {
    h264_header.packetization_type = kH264SingleNalu;
    nalus.push_back(payload_data);
  }
  h264_header.nalu_type = nal_type;
  parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameDelta;

  for (auto& nal : nalus) {
    NaluInfo nalu;
    nalu.type = nal[0] & kH264TypeMask;
    nalu.sps_id = -1;
    nalu.pps_id = -1;
    rtc::ArrayView<const uint8_t> nalu_data = nal.subview(H264::kNaluTypeSize);
    switch (nalu.type) {
      case H264::NaluType::kSps: {
        // Check if VUI is present in SPS and if it needs to be modified to
        // avoid
        // excessive decoder latency.
        std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(nalu_data);

        if (!sps) {
          RTC_LOG(LS_WARNING) << "Failed to parse SPS NAL unit.";
          return std::nullopt;
        }

        nalu.sps_id = sps->id;
        parsed_payload->video_header.width = sps->width;
        parsed_payload->video_header.height = sps->height;
        parsed_payload->video_header.frame_type =
            VideoFrameType::kVideoFrameKey;
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
          parsed_payload->video_header.is_first_packet_in_frame &=
              slice_header->first_mb_in_slice == 0;
        } else {
          RTC_LOG(LS_WARNING) << "Failed to parse PPS id from slice of type: "
                              << static_cast<int>(nalu.type);
          return std::nullopt;
        }
        break;
      }
      // Slices below don't contain SPS or PPS ids.
      case H264::NaluType::kAud:
      case H264::NaluType::kEndOfSequence:
      case H264::NaluType::kEndOfStream:
      case H264::NaluType::kFiller:
      case H264::NaluType::kSei:
        break;
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
  if (rtp_payload.size() < kFuAHeaderSize) {
    RTC_LOG(LS_ERROR) << "FU-A NAL units truncated.";
    return std::nullopt;
  }
  std::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload(
      std::in_place);
  uint8_t original_nal_type = rtp_payload.cdata()[1] & kH264TypeMask;
  bool first_fragment = (rtp_payload.cdata()[1] & kH264SBit) > 0;
  bool is_first_packet_in_frame = false;
  NaluInfo nalu;
  nalu.type = original_nal_type;
  nalu.sps_id = -1;
  nalu.pps_id = -1;
  if (first_fragment) {
    if (original_nal_type == H264::NaluType::kIdr ||
        original_nal_type == H264::NaluType::kSlice) {
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
      }
    }
  }

  if (original_nal_type == H264::NaluType::kIdr) {
    parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameKey;
  } else {
    parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameDelta;
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
  h264_header.nalu_type = original_nal_type;
  if (first_fragment) {
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
    // We handle STAP-A and single NALU's the same way here.
    return ProcessStapAOrSingleNalu(std::move(rtp_payload));
  }
}

rtc::scoped_refptr<EncodedImageBuffer> VideoRtpDepacketizerH264::AssembleFrame(
    rtc::ArrayView<const rtc::ArrayView<const uint8_t>> rtp_payloads) {
  rtc::ByteBufferWriter writer;
  size_t frame_size = 0;

  // Calculate approx frame size to avoid doing extra reallocs.
  for (rtc::ArrayView<const uint8_t> rtp_payload : rtp_payloads) {
    uint8_t nal_type = rtp_payload[0] & kH264TypeMask;
    switch (nal_type) {
      case H264::NaluType::kFuA:
        if (rtp_payload.size() < kFuAHeaderSize) {
          return nullptr;
        }
        frame_size += rtp_payload.size() - kFuAHeaderSize;
        if (rtp_payload[1] & kH264SBit) {
          frame_size += sizeof(start_code_h264) + 1;
        }
        break;
      case H264::NaluType::kStapA: {
        std::vector<rtc::ArrayView<const uint8_t>> nalus;
        if (!ParseStapA(rtp_payload, &nalus)) {
          RTC_LOG(LS_ERROR)
              << "StapA packet with incorrect NALU packet lengths.";
          return nullptr;
        }

        for (auto& nal : nalus) {
          frame_size += sizeof(start_code_h264) + nal.size();
        }
        break;
      }
      default:
        frame_size += sizeof(start_code_h264) + rtp_payload.size();
    }
  }
  for (auto& sps : sps_data_) {
    frame_size += sizeof(start_code_h264) + sps.size();
  }
  for (auto& pps : pps_data_) {
    frame_size += sizeof(start_code_h264) + pps.size();
  }
  writer.Resize(frame_size);

  bool inserted_sps = !sps_data_.empty();
  bool inserted_pps = !pps_data_.empty();
  bool modified_sps_vui = false;

  for (rtc::ArrayView<const uint8_t> rtp_payload : rtp_payloads) {
    uint8_t nal_type = rtp_payload[0] & kH264TypeMask;
    switch (nal_type) {
      case H264::NaluType::kFuA: {
        if (rtp_payload.size() < kFuAHeaderSize) {
          return nullptr;
        }
        rtc::ArrayView<const uint8_t> nal_data =
            rtp_payload.subview(kFuAHeaderSize);

        if (rtp_payload[1] & kH264SBit) {
          uint8_t fnri = rtp_payload[0] & (kH264FBit | kH264NriMask);
          uint8_t original_nal_type = rtp_payload[1] & kH264TypeMask;
          uint8_t original_nal_header = fnri | original_nal_type;
          writer.WriteData(start_code_h264);
          writer.WriteUInt8(original_nal_header);

          if (original_nal_type == H264::NaluType::kIdr) {
            if (!inserted_sps) {
              // Insert out of band sps.
              for (auto& sps : sps_data_) {
                writer.WriteData(start_code_h264);
                writer.WriteData(sps);
              }
              inserted_sps = true;
            }
            if (!inserted_pps) {
              // Insert out of band pps.
              for (auto& pps : pps_data_) {
                writer.WriteData(start_code_h264);
                writer.WriteData(pps);
              }
              inserted_pps = true;
            }
          }
        }
        writer.WriteData(nal_data);
        break;
      }
      case H264::NaluType::kStapA: {
        std::vector<rtc::ArrayView<const uint8_t>> nalus;
        if (!ParseStapA(rtp_payload, &nalus)) {
          RTC_LOG(LS_ERROR)
              << "StapA packet with incorrect NALU packet lengths.";
          return nullptr;
        }
        for (auto& nal : nalus) {
          writer.WriteData(start_code_h264);
          writer.WriteData(nal);
        }
        break;
      }
      case H264::NaluType::kSps: {
        rtc::Buffer output_buffer;
        std::optional<SpsParser::SpsState> sps;

        SpsVuiRewriter::ParseResult result = SpsVuiRewriter::ParseAndRewriteSps(
            rtp_payload, &sps, nullptr, &output_buffer,
            SpsVuiRewriter::Direction::kIncoming);
        switch (result) {
          case SpsVuiRewriter::ParseResult::kFailure:
            RTC_LOG(LS_WARNING) << "Failed to parse SPS NAL unit.";
            return nullptr;
          case SpsVuiRewriter::ParseResult::kVuiRewritten:
            if (modified_sps_vui) {
              RTC_LOG(LS_WARNING)
                  << "More than one H264 SPS NAL units needing "
                     "rewriting found within a single STAP-A packet. "
                     "Keeping the first and rewriting the last.";
            }
            modified_sps_vui = true;
            writer.WriteData(start_code_h264);
            writer.WriteData(output_buffer);
            break;
          case SpsVuiRewriter::ParseResult::kVuiOk:
            writer.WriteData(start_code_h264);
            writer.WriteData(rtp_payload);
            break;
        }
        if (!inserted_sps) {
          // Insert out of band sps
          for (auto& sps : sps_data_) {
            writer.WriteData(start_code_h264);
            writer.WriteData(sps);
          }
          inserted_sps = true;
        }

        break;
      }
      case H264::NaluType::kIdr: {
        if (!inserted_sps) {
          // Insert out of band sps.
          for (auto& sps : sps_data_) {
            writer.WriteData(start_code_h264);
            writer.WriteData(sps);
          }
          inserted_sps = true;
        }
        if (!inserted_pps) {
          // Insert out of band pps.
          for (auto& pps : pps_data_) {
            writer.WriteData(start_code_h264);
            writer.WriteData(pps);
          }
          inserted_pps = true;
        }
        writer.WriteData(start_code_h264);
        writer.WriteData(rtp_payload);
        break;
      }
      default:
        writer.WriteData(start_code_h264);
        writer.WriteData(rtp_payload);
    }
  }
  rtc::Buffer buffer = writer.Finish();
  return webrtc::EncodedImageBuffer::Create(std::move(buffer));
}

void VideoRtpDepacketizerH264::InsertSpsPpsNalus(
    const std::vector<uint8_t>& sps,
    const std::vector<uint8_t>& pps) {
  if (sps.size() < kNalHeaderSize) {
    RTC_LOG(LS_WARNING) << "SPS size  " << sps.size() << " is smaller than "
                        << kNalHeaderSize;
    return;
  }
  if ((sps[0] & 0x1f) != H264::NaluType::kSps) {
    RTC_LOG(LS_WARNING) << "SPS Nalu header missing";
    return;
  }
  if (pps.size() < kNalHeaderSize) {
    RTC_LOG(LS_WARNING) << "PPS size  " << pps.size() << " is smaller than "
                        << kNalHeaderSize;
    return;
  }
  if ((pps[0] & 0x1f) != H264::NaluType::kPps) {
    RTC_LOG(LS_WARNING) << "SPS Nalu header missing";
    return;
  }
  std::optional<SpsParser::SpsState> parsed_sps = SpsParser::ParseSps(
      rtc::ArrayView<const uint8_t>(sps).subview(kNalHeaderSize));
  std::optional<PpsParser::PpsState> parsed_pps = PpsParser::ParsePps(
      rtc::ArrayView<const uint8_t>(pps).subview(kNalHeaderSize));

  if (!parsed_sps) {
    RTC_LOG(LS_WARNING) << "Failed to parse SPS.";
  }

  if (!parsed_pps) {
    RTC_LOG(LS_WARNING) << "Failed to parse PPS.";
  }

  if (!parsed_pps || !parsed_sps) {
    return;
  }

  sps_data_.emplace_back(sps.data(), sps.size());
  pps_data_.emplace_back(pps.data(), pps.size());

  RTC_LOG(LS_INFO) << "Inserted SPS id " << parsed_sps->id << " and PPS id "
                   << parsed_pps->id << " (referencing SPS "
                   << parsed_pps->sps_id << ")";
}

}  // namespace webrtc
