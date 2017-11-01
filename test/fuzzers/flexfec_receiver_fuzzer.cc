/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/flexfec_receiver.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/basictypes.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

constexpr size_t kMinInputSize = 22;
constexpr size_t kMaxPayloadSize = 50;

static uint8_t packet[kRtpHeaderSize + kMaxPayloadSize];

class DummyCallback : public RecoveredPacketReceiver {
  void OnRecoveredPacket(const uint8_t* packet, size_t length) override {
    RTC_CHECK(packet);
  }
};
}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size < kMinInputSize) {
    return;
  }

  size_t i = 0;

  // Base data for RTP headers.
  packet[0] = 1 << 7;  // RTP version 2. No padding, extensions, CSRCs.
  uint8_t flexfec_payload_type;
  memcpy(&flexfec_payload_type, data + i, 1);
  i += 1;
  uint16_t flexfec_seq_num;
  memcpy(&flexfec_seq_num, data + i, 2);
  i += 2;
  uint32_t flexfec_timestamp;
  memcpy(&flexfec_timestamp, data + i, 4);
  i += 4;
  uint32_t flexfec_ssrc;
  memcpy(&flexfec_ssrc, data + i, 4);
  i += 4;
  uint8_t media_payload_type;
  memcpy(&media_payload_type, data + i, 1);
  i += 1;
  uint16_t media_seq_num;
  memcpy(&media_seq_num, data + i, 2);
  i += 2;
  const uint16_t original_media_seq_num = media_seq_num;
  uint32_t media_timestamp;
  memcpy(&media_timestamp, data + i, 4);
  i += 4;
  uint32_t media_ssrc;
  memcpy(&media_ssrc, data + i, 4);
  i += 4;
  RTC_DCHECK_EQ(i, kMinInputSize);

  // Add packets until we run out of data.
  DummyCallback callback;
  FlexfecReceiver receiver(flexfec_ssrc, media_ssrc, &callback);
  while (true) {
    // Simulate RTP header explicitly.
    if (i >= size)
      break;
    bool is_flexfec = false;
    if (data[i++] % 3 == 0) {
      is_flexfec = true;
      ByteWriter<uint8_t>::WriteBigEndian(packet + 1, flexfec_payload_type);
      packet[1] &= ~(1 << 7);  // Marker bit unset.
      ByteWriter<uint16_t>::WriteBigEndian(packet + 2, flexfec_seq_num);
      ++flexfec_seq_num;
      ByteWriter<uint32_t>::WriteBigEndian(packet + 4, flexfec_timestamp);
      flexfec_timestamp += 3000;
      ByteWriter<uint32_t>::WriteBigEndian(packet + 8, flexfec_ssrc);
    } else {
      ByteWriter<uint8_t>::WriteBigEndian(packet + 1, media_payload_type);
      packet[1] |= (1 << 7);  // Marker bit set.
      ByteWriter<uint16_t>::WriteBigEndian(packet + 2, media_seq_num);
      ++media_seq_num;
      ByteWriter<uint32_t>::WriteBigEndian(packet + 4, media_timestamp);
      media_timestamp += 3000;
      ByteWriter<uint32_t>::WriteBigEndian(packet + 8, media_ssrc);
    }

    // Simulate early/late packets by sometimes rewriting the sequence number.
    if (i + 2 >= size)
      break;
    if (data[i++] % 15 == 0) {
      uint16_t reordered_seq_num;
      memcpy(&reordered_seq_num, data + i, 2);
      i += 2;
      ByteWriter<uint16_t>::WriteBigEndian(packet + 2, reordered_seq_num);
    }

    // RTP payload.
    if (i >= size)
      break;
    size_t payload_size = data[i++] % kMaxPayloadSize;
    if (i + payload_size - 1 >= size)
      break;
    memcpy(packet + kRtpHeaderSize, data + i, payload_size);
    i += payload_size;

    // Override parts of FEC header.
    if (is_flexfec) {
      // Clear R bit.
      packet[kRtpHeaderSize] &= ~(1 << 7);
      // Clear F bit.
      packet[kRtpHeaderSize] &= ~(1 << 6);
      // SSRCCount.
      ByteWriter<uint8_t>::WriteBigEndian(packet + kRtpHeaderSize + 8, 1);
      // SSRC_i.
      ByteWriter<uint32_t>::WriteBigEndian(packet + kRtpHeaderSize + 12,
                                           media_ssrc);
      // SN base_i.
      ByteWriter<uint16_t>::WriteBigEndian(packet + kRtpHeaderSize + 16,
                                           original_media_seq_num);
    }

    // Receive simulated packet.
    RtpPacketReceived parsed_packet;
    if (parsed_packet.Parse(packet, payload_size)) {
      receiver.OnRtpPacket(parsed_packet);
    }
  }
}

}  // namespace webrtc
