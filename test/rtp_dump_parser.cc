/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/rtp_dump_parser.h"

#include <algorithm>
#include <limits>

#include "rtc_base/logging.h"

namespace webrtc {
namespace test {

std::unique_ptr<RtpDumpParser> RtpDumpParser::Create(
    rtc::ArrayView<const uint8_t> rtp_dump_buffer) {
  // WrapUnique is used because RtpDumpParser is a private constructor.
  auto rtp_dump_parser = absl::WrapUnique(new RtpDumpParser(rtp_dump_buffer));
  if (!rtp_dump_parser->ParseHeader()) {
    return nullptr;
  }
  return rtp_dump_parser;
}

bool RtpDumpParser::NextPacket(test::RtpPacket* packet) {
  const absl::optional<uint16_t> packet_length_opt = ReadUInt16();
  if (!packet_length_opt.has_value()) {
    RTC_LOG(LS_ERROR) << "Unable to parse packet length from payload.";
    return false;
  }
  if (*packet_length_opt > RtpPacket::kMaxPacketBufferSize) {
    RTC_LOG(LS_ERROR) << "Expected packet length is larger than the maximum "
                         "packet buffer size.";
    return false;
  }
  const absl::optional<uint16_t> original_packet_length_opt = ReadUInt16();
  if (!original_packet_length_opt.has_value()) {
    RTC_LOG(LS_ERROR) << "Unable to parse original packet length from payload.";
    return false;
  }
  const absl::optional<uint32_t> time_ms_opt = ReadUInt32();
  if (!time_ms_opt.has_value()) {
    RTC_LOG(LS_ERROR) << "Unable to parse time ms from payload.";
    return false;
  }

  if (std::numeric_limits<size_t>::max() - *packet_length_opt < read_offset_) {
    RTC_LOG(LS_ERROR)
        << "User provided payload size caused an integer overflow when added "
           "to the current read offset into the buffer.";
    return false;
  }
  const size_t packet_end_offset = read_offset_ + *packet_length_opt;
  if (packet_end_offset >= rtp_dump_buffer_.size()) {
    RTC_LOG(LS_ERROR) << "Expected packet length is larger than the remaining "
                         "rtp dump buffer.";
    return false;
  }

  std::copy(rtp_dump_buffer_.data() + read_offset_,
            rtp_dump_buffer_.data() + packet_end_offset, packet->data);
  packet->length = *packet_length_opt;
  packet->original_length = *original_packet_length_opt;
  packet->time_ms = *time_ms_opt;
  read_offset_ = packet_end_offset;

  return true;
}

bool RtpDumpParser::ParseHeader() {
  if (rtp_dump_buffer_.data() == nullptr) {
    RTC_LOG(LS_ERROR) << "Parsing header failed buffer is nullptr.";
    return false;
  }
  if (rtp_dump_buffer_.size() < kRtpDumpHeaderByteSize) {
    RTC_LOG(LS_ERROR) << "Parsing header line failed buffer is too small.";
    return false;
  }

  // Skip the initial line as this is simply a string that we can ignore.
  read_offset_ = kRtpDumpHeaderByteSize;

  // Read the initial header information and fail if any of it is not parsed.
  const absl::optional<uint32_t> start_sec_opt = ReadUInt32();
  if (!start_sec_opt.has_value()) {
    RTC_LOG(LS_ERROR) << "Unable to parse start_sec from header.";
  }
  const absl::optional<uint32_t> start_usec_opt = ReadUInt32();
  if (!start_usec_opt.has_value()) {
    RTC_LOG(LS_ERROR) << "Unable to parse start_usec from header.";
  }
  const absl::optional<uint16_t> source = ReadUInt32();
  if (!source.has_value()) {
    RTC_LOG(LS_ERROR) << "Unable to parse source from header.";
  }
  const absl::optional<uint16_t> port = ReadUInt16();
  if (!port.has_value()) {
    RTC_LOG(LS_ERROR) << "Unable to parse port from header.";
  }
  const absl::optional<uint16_t> padding = ReadUInt16();
  if (!padding.has_value()) {
    RTC_LOG(LS_ERROR) << "Unable to parse padding from header.";
  }

  return true;
}

absl::optional<uint32_t> RtpDumpParser::ReadUInt32() {
  if (read_offset_ + sizeof(uint32_t) < rtp_dump_buffer_.size()) {
    const uint32_t value = (rtp_dump_buffer_[read_offset_] << 24) +
                           (rtp_dump_buffer_[read_offset_ + 1] << 16) +
                           (rtp_dump_buffer_[read_offset_ + 2] << 8) +
                           rtp_dump_buffer_[read_offset_ + 3];
    read_offset_ += sizeof(uint32_t);
    return value;
  }
  return absl::nullopt;
}

absl::optional<uint16_t> RtpDumpParser::ReadUInt16() {
  if (read_offset_ + sizeof(uint16_t) < rtp_dump_buffer_.size()) {
    const uint16_t value = (rtp_dump_buffer_[read_offset_] << 8) +
                           rtp_dump_buffer_[read_offset_ + 1];
    read_offset_ += sizeof(uint16_t);
    return value;
  }
  return absl::nullopt;
}

RtpDumpParser::RtpDumpParser(rtc::ArrayView<const uint8_t> rtp_dump_buffer)
    : rtp_dump_buffer_(rtp_dump_buffer) {}

}  // namespace test
}  // namespace webrtc
