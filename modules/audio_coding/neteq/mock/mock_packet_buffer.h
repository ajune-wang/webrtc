/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_PACKET_BUFFER_H_
#define MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_PACKET_BUFFER_H_

#include "modules/audio_coding/neteq/packet_buffer.h"
#include "test/gmock.h"

namespace webrtc {

class MockPacketBuffer : public PacketBuffer {
 public:
  MockPacketBuffer(size_t max_number_of_packets, const TickTimer* tick_timer)
      : PacketBuffer(max_number_of_packets, tick_timer) {}
  virtual ~MockPacketBuffer() { Die(); }
  MOCK_METHOD(void, Die, (), (override));
  MOCK_METHOD(void, Flush, (), (override));
  MOCK_METHOD(bool, Empty, (), (const, override));
  int InsertPacket(Packet&& packet, StatisticsCalculator* stats) {
    return InsertPacketWrapped(&packet, stats);
  }
  // Since gtest does not properly support move-only types, InsertPacket is
  // implemented as a wrapper. You'll have to implement InsertPacketWrapped
  // instead and move from |*packet|.
  MOCK_METHOD(int,
              InsertPacketWrapped,
              (Packet*, StatisticsCalculator*),
              (override));
  MOCK_METHOD(int,
              InsertPacketList,
              (PacketList*,
               const DecoderDatabase& decoder_database,
               absl::optional<uint8_t>*,
               absl::optional<uint8_t>*,
               StatisticsCalculator*),
              (override));
  MOCK_METHOD(int, NextTimestamp, (uint32_t*), (const, override));
  MOCK_METHOD(int,
              NextHigherTimestamp,
              (uint32_t timestamp, uint32_t*),
              (const, override));
  MOCK_METHOD(const Packet*, PeekNextPacket, (), (const, override));
  MOCK_METHOD(absl::optional<Packet>, GetNextPacket, (), (override));
  MOCK_METHOD(int, DiscardNextPacket, (StatisticsCalculator*), (override));
  MOCK_METHOD(void,
              DiscardOldPackets,
              (uint32_t timestamp_limit,
               uint32_t horizon_samples,
               StatisticsCalculator*),
              (override));
  MOCK_METHOD(void,
              DiscardAllOldPackets,
              (uint32_t timestamp_limit, StatisticsCalculator*),
              (override));
  MOCK_METHOD(size_t, NumPacketsInBuffer, (), (const, override));
  MOCK_METHOD(void, IncrementWaitingTimes, (int), (override));
  MOCK_METHOD(int, current_memory_bytes, (), (const, override));
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_PACKET_BUFFER_H_
