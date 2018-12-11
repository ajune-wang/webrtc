/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_RTP_PACKET_H_
#define TEST_RTP_PACKET_H_

namespace webrtc {
namespace test {

struct RtpPacket final {
  // Accommodate for 50 ms packets of 32 kHz PCM16 samples (3200 bytes) plus
  // some overhead.
  static constexpr size_t kMaxPacketBufferSize = 3500;
  uint8_t data[kMaxPacketBufferSize] = {0};
  size_t length = 0;
  // The length the packet had on wire. Will be different from |length| when
  // reading a header-only RTP dump.
  size_t original_length = 0;
  uint32_t time_ms = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_RTP_PACKET_H_
