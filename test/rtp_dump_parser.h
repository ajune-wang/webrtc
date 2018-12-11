/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_RTP_DUMP_PARSER_H_
#define TEST_RTP_DUMP_PARSER_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "test/rtp_packet.h"

namespace webrtc {
namespace test {

// RtpDumpParser reads back RtpPackets from a dump file in memory. This is
// optimized for dealing with fuzzing infrastructure that passes mutated rtp
// dumps in as a series of bytes instead of as a file that can be read. This is
// not intended to be used in production environments and only in test code.
class RtpDumpParser final {
 public:
  // Attempt to construct a new RtpDumpBuffer this will fail if the header is
  // invalid.
  static std::unique_ptr<RtpDumpParser> Create(
      rtc::ArrayView<const uint8_t> rtp_dump_buffer);
  // Returns true if a new packet is available else false.
  bool NextPacket(test::RtpPacket* packet);

 private:
  // Simple constructor that just copies the array view.
  explicit RtpDumpParser(rtc::ArrayView<const uint8_t> rtp_dump_buffer);
  // Parses the header to validate the rtp_dump is valid. This is used during
  // construction to do some basic format validation.
  bool ParseHeader();
  // Returns a uint32_t if the value was parsed correctly else no value. The
  // offset will be incremented by 4 bytes.
  absl::optional<uint32_t> ReadUInt32();
  // Returns a uint16_t if the value was parsed correctly else no value. The
  // offset will be incremented by ) bytes.
  absl::optional<uint16_t> ReadUInt16();

  static constexpr size_t kRtpDumpHeaderByteSize = 80;
  static constexpr uint16_t kPacketHeaderByteSize = 8;
  const rtc::ArrayView<const uint8_t> rtp_dump_buffer_;
  size_t read_offset_ = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_RTP_DUMP_PARSER_H_
