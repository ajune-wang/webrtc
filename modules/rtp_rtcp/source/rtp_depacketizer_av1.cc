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

#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
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
constexpr int kObuTypeSequenceHeader = 1;

struct Leb128Value {
  // Number of bytes used to store the |value|.
  size_t size = 0;
  // Encoded value.
  size_t value = 0;
};

int ObuType(uint8_t obu_header) {
  return (obu_header & 0b0'1111'000) >> 3;
}

bool RtpStartsWithFragment(uint8_t aggregation_header) {
  return aggregation_header & 0b1000'0000;
}
bool RtpEndsWithFragment(uint8_t aggregation_header) {
  return aggregation_header & 0b0100'0000;
}
int RtpNumObus(uint8_t aggregation_header) {  // 0 for any number of obus.
  return (aggregation_header & 0b0011'0000) >> 4;
}

// Reads the leb128 encoded value. Returns .size = 0 on error.
Leb128Value ReadLeb128(rtc::ArrayView<const uint8_t> next_bytes) {
  Leb128Value result;
  for (uint8_t byte : next_bytes) {
    result.value |= (byte & 0b0111'1111) << (result.size * 7);
    ++result.size;
    if ((byte & 0b1000'0000) == 0) {
      return result;
    }
  }
  // Error: terminator byte not found.
  result.size = 0;
  return result;
}

}  // namespace

bool RtpDepacketizerAv1::Parse(ParsedPayload* parsed_payload,
                               const uint8_t* payload_data,
                               size_t payload_data_length) {
  RTC_DCHECK(parsed_payload);
  if (payload_data_length == 0) {
    RTC_DLOG(LS_ERROR) << "Empty rtp payload.";
    return false;
  }
  uint8_t aggregation_header = payload_data[0];

  // To assemble frame, all of the rtp payload is required, including
  // aggregation header.
  parsed_payload->payload = payload_data;
  parsed_payload->payload_length = payload_data_length;

  // TODO(danilchap): Set AV1 codec when there is such enum value
  parsed_payload->video.codec = VideoCodecType::kVideoCodecGeneric;
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
        Leb128Value size = ReadLeb128(remaining);
        if (size.size == 0) {
          RTC_DLOG(LS_WARNING)
              << "Failed to read OBU fragment size for OBU#" << obu_index;
          return false;
        }
        remaining = remaining.subview(size.size);
        fragment_size = size.value;
        if (fragment_size > remaining.size()) {
          RTC_DLOG(LS_WARNING) << "OBU fragment size " << fragment_size
                               << " exceeds remaining payload size "
                               << remaining.size() << " for OBU#" << obu_index;
          // Malformed input: written size is larger than remaining buffer.
          return false;
        }
      } else {
        fragment_size = remaining.size();
      }
      // Though it is inpractical to pass empty fragments, it is still allowed.
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
