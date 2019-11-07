/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_depacketizer_av1.h"

#include <stdint.h>

#include <algorithm>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/scoped_refptr.h"
#include "api/video/encoded_image.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
// AV1 format:
//
// RTP packet syntax:
//
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |Z|Y| W |-|-|-|-| (REQUIRED)
//      +=+=+=+=+=+=+=+=+ (REPEATED (W-1) times, or any times if W = 0)
//      |1|             |
//      +-+ OBU fragment|
// S   :|1|             | (REQUIRED, leb128 encoded)
//      +-+    size     |
//      |0|             |
//      +-+-+-+-+-+-+-+-+
//      |  OBU fragment |
//      |     ...       |
//      +=+=+=+=+=+=+=+=+
//      |     ...       |
//      +=+=+=+=+=+=+=+=+ if W > 0, last fragment MUST NOT have size field
//      |  OBU fragment |
//      |     ...       |
//      +=+=+=+=+=+=+=+=+
//
//
// OBU syntax:
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |0| type  |X|S|-| (REQUIRED)
//      +-+-+-+-+-+-+-+-+
// X:   | TID |SID|-|-|-| (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
//      |1|             |
//      +-+ OBU payload |
// S:   |1|             | (OPTIONAL, variable length leb128 encoded)
//      +-+    size     |
//      |0|             |
//      +-+-+-+-+-+-+-+-+
//      |  OBU payload  |
//      |     ...       |
constexpr uint8_t kObuHasSizeBit = 0b0'0000'010;
constexpr int kObuTypeSequenceHeader = 1;

struct Leb128Value {
  // Number of bytes used to store the |value|.
  size_t size = 0;
  // Encoded value.
  size_t value = 0;
};

struct ObuInfo {
  size_t data_size = 0;
  size_t size_size = 0;
  // Size of the obu if it was present in the bitstream. 0 otherwise.
  // Since that arrived from the network it can't be trusted, but can be
  // validated.
  size_t signaled_size = 0;
  // OBU payloads excluding size fields if they were provided.
  absl::InlinedVector<rtc::ArrayView<const uint8_t>, 2> data;
};

bool ObuHasExtension(uint8_t obu_header) {
  return obu_header & 0b0'0000'100;
}
bool ObuHasSize(uint8_t obu_header) {
  return obu_header & 0b0'0000'010;
}
int ObuType(uint8_t obu_header) {
  return (obu_header & 0b0'1111'000) >> 3;
}

bool RtpStartsWithFragment(uint8_t aggregation_header) {
  return aggregation_header & 0b1000'0000;
}
// aka "to be continued..."
bool RtpEndsWithFragment(uint8_t aggregation_header) {
  return aggregation_header & 0b0100'0000;
}
int RtpNumObus(uint8_t aggregation_header) {  // 0 for any number of obus.
  return (aggregation_header & 0b0011'0000) >> 4;
}

size_t BytesToStoreSize(size_t size) {
  if (size == 0)
    return 1;
  size_t result = 0;
  while (size > 0) {
    result++;
    size >>= 7;
  }
  return result;
}

// Writes the |size| in leb128 format. returns number of bytes used.
// Assumes buffer is large enough to store the size.
size_t WriteLeb128(uint8_t* buffer, size_t value) {
  RTC_DCHECK_LT(value, size_t{1} << 32);
  size_t offset = 0;
  while (value >= 0x80) {
    buffer[offset] = 0x80 | (value & 0x7F);
    ++offset;
    value >>= 7;
  }
  buffer[offset] = value;
  ++offset;
  return offset;
}

// Reads the leb128 encoded value. Returns .size = 0 on error.
Leb128Value ReadLeb128(rtc::FunctionView<absl::optional<uint8_t>()> next_byte) {
  Leb128Value result;
  while (absl::optional<uint8_t> byte = next_byte()) {
    result.value |= (*byte & 0x7F) << (result.size * 7);
    ++result.size;
    if ((*byte & 0x80) == 0) {
      return result;
    }
  }
  // Error: terminator byte not found.
  result.size = 0;
  return result;
}

// Reads the offset and size (if present) of the obu payload (i.e. obu excluding
// obu_header and obu_size fields) Returns .size = 0 on error.
Leb128Value ReadSize(const ObuInfo& obu) {
  Leb128Value result;
  RTC_DCHECK(!obu.data.empty());
  RTC_DCHECK(!obu.data[0].empty());

  // (packet_it, byte_it) tuple is an iterator over the array or arrays.
  auto packet_it = obu.data.begin();
  auto byte_it = packet_it->begin();
  auto next_byte = [&]() -> absl::optional<uint8_t> {
    if (packet_it == obu.data.end()) {
      return absl::nullopt;
    }
    uint8_t result = *byte_it;
    if (++byte_it == packet_it->end()) {
      ++packet_it;
      if (packet_it != obu.data.end()) {
        RTC_DCHECK(!packet_it->empty());
        byte_it = packet_it->begin();
      }
    }
    return result;
  };

  uint8_t obu_header = *next_byte();
  if (ObuHasExtension(obu_header)) {
    // skip it before reading the size.
    if (next_byte() == absl::nullopt) {
      // Failed to read obu_extension_header: malformed OBU.
      result.size = 0;
      return result;
    }
  }
  RTC_DCHECK(ObuHasSize(obu_header));
  return ReadLeb128(next_byte);
}

}  // namespace

rtc::scoped_refptr<EncodedImageBuffer> RtpDepacketizerAv1::AssembleFrame(
    rtc::ArrayView<const rtc::ArrayView<const uint8_t>> rtp_payloads) {
  absl::InlinedVector<ObuInfo, 4> obus;
  bool expect_continues_obu = false;
  for (rtc::ArrayView<const uint8_t> rtp_payload : rtp_payloads) {
    if (rtp_payload.empty()) {
      RTC_DLOG(WARNING) << "Failed to find aggregation header in a packet";
      return nullptr;
    }
    uint8_t aggregation_header = rtp_payload[0];
    // Z-bit: 1 if the first OBU contained in the packet is a continuation of a
    // previous OBU.
    bool continues_obu = RtpStartsWithFragment(aggregation_header);
    if (continues_obu != expect_continues_obu) {
      RTC_DLOG(WARNING) << "Unexpected Z-bit " << continues_obu;
      return nullptr;
    }
    int num_expected_obus = RtpNumObus(aggregation_header);

    rtc::ArrayView<const uint8_t> remaining;
    int obu_index;
    for (remaining = rtp_payload.subview(1), obu_index = 1; !remaining.empty();
         continues_obu = false, ++obu_index) {
      ObuInfo& obu = continues_obu ? obus.back() : obus.emplace_back();
      size_t fragment_size;
      bool has_fragment_size = (obu_index != num_expected_obus);
      if (has_fragment_size) {
        fragment_size = ReadLeb128([&remaining]() -> absl::optional<uint8_t> {
                          if (remaining.empty()) {
                            return absl::nullopt;
                          }
                          uint8_t next_byte = remaining[0];
                          remaining = remaining.subview(1);
                          return next_byte;
                        }).value;
        if (fragment_size > remaining.size()) {
          // Malformed input: written size is larger than remaining buffer.
          RTC_DLOG(WARNING) << "Malformed fragment size " << fragment_size
                            << " is larger than remaining size "
                            << remaining.size() << " while reading obu #"
                            << obu_index << "/" << num_expected_obus;
          return nullptr;
        }
      } else {
        fragment_size = remaining.size();
      }
      // While it is in-practical to pass empty fragments, it is still possible.
      if (fragment_size > 0) {
        obu.data_size += fragment_size;
        obu.data.push_back(remaining.subview(0, fragment_size));
        remaining = remaining.subview(fragment_size);
      }
    }

    // Z flag should be same as Y flag of the next packet.
    expect_continues_obu = RtpEndsWithFragment(aggregation_header);
  }
  if (expect_continues_obu) {
    RTC_DLOG(WARNING) << "Last packet shouldn't have last obu fragmented.";
    return nullptr;
  }

  size_t frame_size = 0;
  for (ObuInfo& obu : obus) {
    RTC_DCHECK(!obu.data.empty());
    RTC_DCHECK(!obu.data[0].empty());
    uint8_t obu_header = obu.data[0][0];
    // At this point obu.size includes everything written in the rtp packet,
    // including header and obu_size(if present). subtract that.
    size_t header_size = ObuHasExtension(obu_header) ? 2 : 1;
    obu.data_size -= header_size;
    if (ObuHasSize(obu_header)) {
      Leb128Value size = ReadSize(obu);
      if (size.size == 0) {
        RTC_DLOG(WARNING) << "Failed to read obu_size.";
        return nullptr;
      }
      obu.signaled_size = size.value;
      obu.data_size -= size.size;
    }
    // now obu.size should have proper size.

    if (obu.signaled_size > 0 && obu.signaled_size != obu.data_size) {
      // obu_size was present in the bitstream and mismatches calculated size.
      RTC_DLOG(WARNING) << "Mismatch in obu_size. signaled: "
                        << obu.signaled_size << ", actual: " << obu.data_size;
      return nullptr;
    }
    obu.size_size = BytesToStoreSize(obu.data_size);

    frame_size += (header_size + obu.size_size + obu.data_size);
  }

  auto bitstream = EncodedImageBuffer::Create(frame_size);
  uint8_t* write_at = bitstream->data();
  for (ObuInfo& obu : obus) {
    uint8_t obu_header = obu.data[0][0];
    // Store obu_header
    *write_at = obu_header | kObuHasSizeBit;
    ++write_at;
    if (ObuHasExtension(obu_header)) {
      *write_at = obu.data[0].size() == 1 ? obu.data[1][0] : obu.data[0][1];
      ++write_at;
    }
    // Store obu_size
    size_t used_bytes = WriteLeb128(write_at, obu.data_size);
    RTC_DCHECK_EQ(used_bytes, obu.size_size);
    write_at += used_bytes;
    // Store rest of the obu.
    uint8_t* const payload_begin = write_at;
    uint8_t* payload_end = payload_begin + obu.data_size;
    // Copy backwards to avoid copying obu_size (if present) and obu_header.
    for (auto packet_it = obu.data.rbegin(); packet_it != obu.data.rend();
         ++packet_it) {
      size_t copy_bytes =
          std::min<size_t>(packet_it->size(), payload_end - payload_begin);
      memcpy(payload_end - copy_bytes,
             packet_it->data() + packet_it->size() - copy_bytes, copy_bytes);
      payload_end -= copy_bytes;
    }
    RTC_DCHECK_EQ(payload_end - payload_begin, 0);
    write_at += obu.data_size;
  }
  RTC_CHECK_EQ(write_at - bitstream->data(), bitstream->size());
  return bitstream;
}

bool RtpDepacketizerAv1::Parse(ParsedPayload* parsed_payload,
                               const uint8_t* payload_data,
                               size_t payload_data_length) {
  RTC_DCHECK(parsed_payload);
  if (payload_data_length == 0)
    return false;
  uint8_t aggregation_header = payload_data[0];

  // To assemble frame, all of the rtp payload is required, including
  // aggregation header.
  parsed_payload->payload = payload_data;
  parsed_payload->payload_length = payload_data_length;

  parsed_payload->video.codec = VideoCodecType::kVideoCodecAV1;
  // These are not accurate since frame may consist of several packet aligned
  // chunks of obus, but should be good enough for most cases. It might produce
  // frame that do not map to any real frame, but av1 decoder should be able to
  // handle it since it promise to handle individual obus rather than full
  // frames.
  parsed_payload->video.is_first_packet_in_frame =
      !RtpStartsWithFragment(aggregation_header);
  parsed_payload->video.is_last_packet_in_frame =
      !RtpEndsWithFragment(aggregation_header);
  parsed_payload->video.frame_type = VideoFrameType::kVideoFrameDelta;
  // If packet starts a frame, check if it contains Sequence Header OBU.
  // In that case treat it as key frame packet.
  if (parsed_payload->video.is_first_packet_in_frame) {
    int num_expected_obus = RtpNumObus(aggregation_header);

    auto remaining =
        rtc::MakeArrayView(payload_data + 1, payload_data_length - 1);
    for (int obu_index = 1; !remaining.empty(); ++obu_index) {
      size_t fragment_size;
      bool has_fragment_size = (obu_index != num_expected_obus);
      if (has_fragment_size) {
        fragment_size = ReadLeb128([&remaining]() -> absl::optional<uint8_t> {
                          if (remaining.empty()) {
                            return absl::nullopt;
                          }
                          uint8_t next_byte = remaining[0];
                          remaining = remaining.subview(1);
                          return next_byte;
                        }).value;
        if (fragment_size > remaining.size()) {
          // Malformed input: written size is larger than remaining buffer.
          return false;
        }
      } else {
        fragment_size = remaining.size();
      }
      // While it is in-practical to pass empty fragments, it is still possible.
      if (fragment_size == 0) {
        continue;
      }
      if (ObuType(remaining[0]) == kObuTypeSequenceHeader) {
        // TODO(danilchap): Check frame_header OBU and/or frame OBU too for
        // other conditions of the start of a new coded video sequence. For
        // proper checks checking single packet might not be enough. See
        // https://aomediacodec.github.io/av1-spec/av1-spec.pdf section 7.5
        parsed_payload->video.frame_type = VideoFrameType::kVideoFrameKey;
        break;
      }
      remaining = remaining.subview(fragment_size);
    }
  }

  return true;
}

}  // namespace webrtc
