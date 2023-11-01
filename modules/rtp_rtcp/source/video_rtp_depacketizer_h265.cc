/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/video_rtp_depacketizer_h265.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/video/video_codec_type.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h265/h265_bitstream_parser.h"
#include "common_video/h265/h265_common.h"
#include "common_video/h265/h265_pps_parser.h"
#include "common_video/h265/h265_sps_parser.h"
#include "common_video/h265/h265_vps_parser.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

// The payload header consists of the same
// fields (F, Type, LayerId and TID) as the NAL unit header. Refer to
// section 4.4 in RFC 7798.
constexpr size_t kH265PayloadHeaderSize = 2;
// Unlike H.264, H265 NAL header is 2-bytes.
constexpr size_t kH265NalHeaderSize = 2;
// H265's FU is constructed of 2-byte payload header, 1-byte FU header and FU
// payload.
constexpr size_t kH265FuHeaderSize = 1;
// The NALU size for H265 RTP aggregated packet indicates the size of the NAL
// unit is 2-bytes.
constexpr size_t kH265LengthFieldSize = 2;
constexpr size_t kH265ApHeaderSize = kH265NalHeaderSize + kH265LengthFieldSize;

enum H265NalHdrMasks {
  kH265FBit = 0x80,
  kH265TypeMask = 0x7E,
  kH265LayerIDHMask = 0x1,
  kH265LayerIDLMask = 0xF8,
  kH265TIDMask = 0x7,
  kH265TypeMaskN = 0x81,
  kH265TypeMaskInFuHeader = 0x3F
};

// Bit masks for FU headers.
enum H265FuBitmasks {
  kH265SBitMask = 0x80,
  kH265EBitMask = 0x40,
  kH265FuTypeBitMask = 0x3F
};

constexpr uint8_t kStartCode[] = {0, 0, 0, 1};

bool ParseApStartOffsets(const uint8_t* nalu_ptr,
                         size_t length_remaining,
                         std::vector<size_t>* offsets) {
  size_t offset = 0;
  while (length_remaining > 0) {
    // Buffer doesn't contain room for additional nalu length.
    if (length_remaining < sizeof(uint16_t))
      return false;
    // Read 16-bit NALU size defined in RFC7798 section 4.4.2.
    uint16_t nalu_size = ByteReader<uint16_t>::ReadBigEndian(nalu_ptr);
    nalu_ptr += sizeof(uint16_t);
    length_remaining -= sizeof(uint16_t);
    if (nalu_size > length_remaining)
      return false;
    nalu_ptr += nalu_size;
    length_remaining -= nalu_size;

    offsets->push_back(offset + kH265ApHeaderSize);
    offset += kH265LengthFieldSize + nalu_size;
  }
  return true;
}

absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> ProcessApOrSingleNalu(
    rtc::CopyOnWriteBuffer rtp_payload) {
  // Skip the single nalu header (payload header), AP case will be checked later
  if (rtp_payload.size() <= kH265PayloadHeaderSize) {
    RTC_LOG(LS_ERROR) << "Single nalu header truncated.";
    return absl::nullopt;
  }
  const uint8_t* const payload_data = rtp_payload.cdata();
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload(
      absl::in_place);
  parsed_payload->video_header.width = 0;
  parsed_payload->video_header.height = 0;
  parsed_payload->video_header.codec = kVideoCodecH265;
  parsed_payload->video_header.is_first_packet_in_frame = true;

  const uint8_t* nalu_start = payload_data + kH265PayloadHeaderSize;
  const size_t nalu_length = rtp_payload.size() - kH265PayloadHeaderSize;
  uint8_t nal_type = (payload_data[0] & kH265TypeMask) >> 1;
  std::vector<size_t> nalu_start_offsets;
  rtc::CopyOnWriteBuffer video_payload;
  if (nal_type == H265::NaluType::kAp) {
    // Skip the AP header (AP NAL type + length).
    if (rtp_payload.size() <= kH265ApHeaderSize) {
      RTC_LOG(LS_ERROR) << "AP header truncated.";
      return absl::nullopt;
    }

    if (!ParseApStartOffsets(nalu_start, nalu_length, &nalu_start_offsets)) {
      RTC_LOG(LS_ERROR) << "AP packet with incorrect NALU packet lengths.";
      return absl::nullopt;
    }

    nal_type = (payload_data[kH265ApHeaderSize] & kH265TypeMask) >> 1;
  } else {
    nalu_start_offsets.push_back(0);
  }
  parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameDelta;

  nalu_start_offsets.push_back(rtp_payload.size() +
                               kH265LengthFieldSize);  // End offset.
  for (size_t i = 0; i < nalu_start_offsets.size() - 1; ++i) {
    size_t start_offset = nalu_start_offsets[i];
    // End offset is actually start offset for next unit, excluding length field
    // so remove that from this units length.
    size_t end_offset = nalu_start_offsets[i + 1] - kH265LengthFieldSize;
    if (end_offset - start_offset < kH265NalHeaderSize) {
      RTC_LOG(LS_ERROR) << "Aggregated packet too short";
      return absl::nullopt;
    }

    // Insert start code before each nalu in AP.
    video_payload.AppendData(kStartCode);
    video_payload.AppendData(&payload_data[start_offset],
                             end_offset - start_offset);

    uint8_t nalu_type = (payload_data[start_offset] & kH265TypeMask) >> 1;
    start_offset += kH265NalHeaderSize;
    switch (nalu_type) {
      case H265::NaluType::kSps: {
        // Copy any previous data first (likely just the first header).
        std::unique_ptr<rtc::Buffer> output_buffer(new rtc::Buffer());
        if (start_offset)
          output_buffer->AppendData(payload_data, start_offset);

        absl::optional<H265SpsParser::SpsState> sps = H265SpsParser::ParseSps(
            &payload_data[start_offset], end_offset - start_offset);

        if (sps) {
          parsed_payload->video_header.width = sps->width;
          parsed_payload->video_header.height = sps->height;
        } else {
          RTC_LOG(LS_WARNING) << "Failed to parse SPS from SPS slice.";
        }
        parsed_payload->video_header.frame_type =
            VideoFrameType::kVideoFrameKey;
      } break;
      case H265::NaluType::kVps:
      case H265::NaluType::kPps:
      case H265::NaluType::kIdrWRadl:
      case H265::NaluType::kIdrNLp:
      case H265::NaluType::kCra:
        parsed_payload->video_header.frame_type =
            VideoFrameType::kVideoFrameKey;
        ABSL_FALLTHROUGH_INTENDED;
      case H265::NaluType::kTrailN:
      case H265::NaluType::kTrailR:
      // Slices below don't contain SPS or PPS ids.
      case H265::NaluType::kAud:
      case H265::NaluType::kTsaN:
      case H265::NaluType::kTsaR:
      case H265::NaluType::kStsaN:
      case H265::NaluType::kStsaR:
      case H265::NaluType::kRadlN:
      case H265::NaluType::kRadlR:
      case H265::NaluType::kBlaWLp:
      case H265::NaluType::kBlaWRadl:
      case H265::NaluType::kPrefixSei:
      case H265::NaluType::kSuffixSei:
        break;
      case H265::NaluType::kAp:
      case H265::NaluType::kFu:
      case H265::NaluType::kPaci:
        RTC_LOG(LS_WARNING) << "Unexpected AP, FU or PACI received.";
        return absl::nullopt;
    }
  }
  parsed_payload->video_payload = video_payload;
  return parsed_payload;
}

absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> ParseFuNalu(
    rtc::CopyOnWriteBuffer rtp_payload) {
  if (rtp_payload.size() < kH265FuHeaderSize + kH265NalHeaderSize) {
    RTC_LOG(LS_ERROR) << "FU NAL units truncated.";
    return absl::nullopt;
  }
  absl::optional<VideoRtpDepacketizer::ParsedRtpPayload> parsed_payload(
      absl::in_place);

  uint8_t f = rtp_payload.cdata()[0] & kH265FBit;
  uint8_t layer_id_h = rtp_payload.cdata()[0] & kH265LayerIDHMask;
  uint8_t layer_id_l_unshifted = rtp_payload.cdata()[1] & kH265LayerIDLMask;
  uint8_t tid = rtp_payload.cdata()[1] & kH265TIDMask;

  uint8_t original_nal_type = rtp_payload.cdata()[2] & kH265TypeMaskInFuHeader;
  bool first_fragment = rtp_payload.cdata()[2] & kH265SBitMask;
  if (first_fragment) {
    rtp_payload = rtp_payload.Slice(kH265FuHeaderSize,
                                    rtp_payload.size() - kH265FuHeaderSize);
    rtp_payload.MutableData()[0] = f | original_nal_type << 1 | layer_id_h;
    rtp_payload.MutableData()[1] = layer_id_l_unshifted | tid;
    rtc::CopyOnWriteBuffer video_payload;
    // Insert start code before the first fragment in FU.
    video_payload.AppendData(kStartCode);
    video_payload.AppendData(rtp_payload);
    parsed_payload->video_payload = video_payload;
  } else {
    parsed_payload->video_payload = rtp_payload.Slice(
        kH265NalHeaderSize + kH265FuHeaderSize,
        rtp_payload.size() - kH265NalHeaderSize - kH265FuHeaderSize);
  }

  if (original_nal_type == H265::NaluType::kIdrWRadl ||
      original_nal_type == H265::NaluType::kIdrNLp ||
      original_nal_type == H265::NaluType::kCra) {
    parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameKey;
  } else {
    parsed_payload->video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  }
  parsed_payload->video_header.width = 0;
  parsed_payload->video_header.height = 0;
  parsed_payload->video_header.codec = kVideoCodecH265;
  parsed_payload->video_header.is_first_packet_in_frame = first_fragment;

  return parsed_payload;
}

}  // namespace

absl::optional<VideoRtpDepacketizer::ParsedRtpPayload>
VideoRtpDepacketizerH265::Parse(rtc::CopyOnWriteBuffer rtp_payload) {
  if (rtp_payload.size() == 0) {
    RTC_LOG(LS_ERROR) << "Empty payload.";
    return absl::nullopt;
  }

  uint8_t nal_type = (rtp_payload.cdata()[0] & kH265TypeMask) >> 1;

  if (nal_type == H265::NaluType::kFu) {
    // Fragmented NAL units (FU).
    return ParseFuNalu(std::move(rtp_payload));
  } else if (nal_type == H265::NaluType::kPaci) {
    // TODO(bugs.webrtc.org/13485): Implement PACI parse for H265
    RTC_LOG(LS_ERROR) << "Not support type:" << nal_type;
    return absl::nullopt;
  } else {
    // Single NAL unit packet or Aggregated packets (AP).
    return ProcessApOrSingleNalu(std::move(rtp_payload));
  }
}

}  // namespace webrtc
