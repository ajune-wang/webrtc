/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/video_rtp_depacketizer_vp8.h"

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

// VP8 format:
//
// Payload descriptor
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |X|R|N|S|PartID | (REQUIRED)
//      +-+-+-+-+-+-+-+-+
// X:   |I|L|T|K|  RSV  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// I:   |   PictureID   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// L:   |   TL0PICIDX   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// T/K: |TID:Y| KEYIDX  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
//
// Payload header (considered part of the actual payload, sent to decoder)
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |Size0|H| VER |P|
//      +-+-+-+-+-+-+-+-+
//      |      ...      |
//      +               +
namespace webrtc {
namespace {

constexpr int kFailedToParse = 0;

// clang-format off
// First byte.
constexpr uint8_t kHasExtension        = 0b1000'0000;
constexpr uint8_t kNonReferenceFlag    = 0b0010'0000;
constexpr uint8_t kStartsPartitionFlag = 0b0001'0000;
constexpr uint8_t kPartitionMask       = 0b0000'1111;
// Extension byte.
constexpr uint8_t kHasPictureId  = 0b1000'0000;
constexpr uint8_t kHasTl0PicIdx  = 0b0100'0000;
constexpr uint8_t kHasTemporalId = 0b0010'0000;
constexpr uint8_t kHasKeyIndex   = 0b0001'0000;
// clang-format on

int ParseVP8Extension(RTPVideoHeaderVP8* vp8,
                      const uint8_t* data,
                      size_t data_length) {
  RTC_DCHECK_GT(data_length, 0);
  int parsed_bytes = 0;
  // Optional X field is present.
  uint8_t extensions = *data;

  // Advance data and decrease remaining payload size.
  data++;
  parsed_bytes++;
  data_length--;

  if (extensions & kHasPictureId) {
    if (data_length == 0)
      return kFailedToParse;

    uint16_t picture_id = *data & 0x7F;
    if (*data & 0x80) {
      data++;
      parsed_bytes++;
      data_length--;
      if (data_length == 0)
        return kFailedToParse;
      // PictureId is 15 bits
      picture_id <<= 8;
      picture_id |= *data;
    }
    vp8->pictureId = picture_id;
    data++;
    parsed_bytes++;
  }

  if (extensions & kHasTl0PicIdx) {
    if (data_length == 0)
      return kFailedToParse;

    vp8->tl0PicIdx = *data;
    data++;
    parsed_bytes++;
    data_length--;
  }

  if (extensions & (kHasTemporalId | kHasKeyIndex)) {
    if (data_length == 0)
      return kFailedToParse;

    if (extensions & kHasTemporalId) {
      vp8->temporalIdx = ((*data >> 6) & 0x03);
      vp8->layerSync = (*data & 0x20) ? true : false;  // Y bit
    }
    if (extensions & kHasKeyIndex) {
      vp8->keyIdx = (*data & 0x1F);
    }
    data++;
    parsed_bytes++;
    data_length--;
  }
  return parsed_bytes;
}

}  // namespace

absl::optional<VideoRtpDepacketizer::ParsedRtpPayload>
VideoRtpDepacketizerVp8::Parse(rtc::CopyOnWriteBuffer rtp_payload) {
  rtc::ArrayView<const uint8_t> payload(rtp_payload.cdata(),
                                        rtp_payload.size());
  absl::optional<ParsedRtpPayload> result(absl::in_place);
  int offset = ParseRtpPayload(payload, &result->video_header);
  if (offset == kFailedToParse)
    return absl::nullopt;
  RTC_DCHECK_LT(offset, rtp_payload.size());
  result->video_payload =
      rtp_payload.Slice(offset, rtp_payload.size() - offset);
  return result;
}

int VideoRtpDepacketizerVp8::ParseRtpPayload(
    rtc::ArrayView<const uint8_t> rtp_payload,
    RTPVideoHeader* video_header) {
  RTC_DCHECK(video_header);
  if (rtp_payload.empty()) {
    RTC_LOG(LS_ERROR) << "Empty payload.";
    return kFailedToParse;
  }
  const uint8_t* payload_data = rtp_payload.data();
  size_t payload_size = rtp_payload.size();

  // Parse mandatory first byte of payload descriptor.
  uint8_t first_byte = *payload_data;
  bool beginning_of_partition = first_byte & kStartsPartitionFlag;
  uint8_t partition_id = first_byte & kPartitionMask;

  video_header->is_first_packet_in_frame =
      beginning_of_partition && partition_id == 0;
  video_header->simulcastIdx = 0;
  video_header->codec = kVideoCodecVP8;

  auto& vp8_header =
      video_header->video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.nonReference = first_byte & kNonReferenceFlag;
  vp8_header.partitionId = partition_id;
  vp8_header.beginningOfPartition = beginning_of_partition;
  vp8_header.pictureId = kNoPictureId;
  vp8_header.tl0PicIdx = kNoTl0PicIdx;
  vp8_header.temporalIdx = kNoTemporalIdx;
  vp8_header.layerSync = false;
  vp8_header.keyIdx = kNoKeyIdx;

  if (partition_id > 8) {
    // Weak check for corrupt payload_data: PartID MUST NOT be larger than 8.
    return kFailedToParse;
  }

  if (payload_size <= 1) {
    RTC_LOG(LS_ERROR) << "Error parsing VP8 payload descriptor!";
    return kFailedToParse;
  }
  // Advance payload_data and decrease remaining payload size.
  payload_data++;
  payload_size--;

  if (first_byte & kHasExtension) {
    const int parsed_bytes =
        ParseVP8Extension(&vp8_header, payload_data, payload_size);
    if (parsed_bytes == kFailedToParse)
      return kFailedToParse;
    payload_data += parsed_bytes;
    payload_size -= parsed_bytes;
    if (payload_size == 0) {
      RTC_LOG(LS_ERROR) << "Error parsing VP8 payload descriptor!";
      return kFailedToParse;
    }
  }

  // Read P bit from payload header (only at beginning of first partition).
  if (beginning_of_partition && partition_id == 0 &&
      (*payload_data & 0x01) == 0) {
    video_header->frame_type = VideoFrameType::kVideoFrameKey;

    if (payload_size < 10) {
      // For an I-frame we should always have the uncompressed VP8 header
      // in the beginning of the partition.
      return kFailedToParse;
    }
    video_header->width = ((payload_data[7] << 8) + payload_data[6]) & 0x3FFF;
    video_header->height = ((payload_data[9] << 8) + payload_data[8]) & 0x3FFF;
  } else {
    video_header->frame_type = VideoFrameType::kVideoFrameDelta;

    video_header->width = 0;
    video_header->height = 0;
  }

  return rtp_payload.size() - payload_size;
}

}  // namespace webrtc
