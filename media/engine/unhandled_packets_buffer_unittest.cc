/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/unhandled_packets_buffer.h"
#include <memory>
#include "absl/memory/memory.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;

namespace {

std::unique_ptr<rtc::CopyOnWriteBuffer> Create(int n) {
  return absl::make_unique<rtc::CopyOnWriteBuffer>(std::to_string(n));
}

}  // namespace

namespace cricket {

// const size_t UnhandledPacketsBuffer::kMaxStashedPackets = 50;

class FakePacketReceiver : public webrtc::PacketReceiver {
 public:
  DeliveryStatus DeliverPacket(webrtc::MediaType media_type,
                               rtc::CopyOnWriteBuffer packet,
                               int64_t packet_time_us) override {
    EXPECT_EQ(packet_time_us, -1);
    packets.push_back(packet);
    return PacketReceiver::DELIVERY_OK;
  }
  std::vector<rtc::CopyOnWriteBuffer> packets;
};

TEST(UnhandledPacketsBuffer, NoPackets) {
  UnhandledPacketsBuffer buff;
  buff.AddPacket(2, *Create(3));

  FakePacketReceiver receiver;
  buff.BackfillPackets({3}, &receiver);
  EXPECT_EQ(0u, receiver.packets.size());
}

TEST(UnhandledPacketsBuffer, OnePackets) {
  UnhandledPacketsBuffer buff;
  buff.AddPacket(2, *Create(3));

  FakePacketReceiver receiver;
  buff.BackfillPackets({2}, &receiver);
  ASSERT_EQ(1u, receiver.packets.size());
  EXPECT_EQ(*Create(3), receiver.packets[0]);
}

TEST(UnhandledPacketsBuffer, TwoPacketsTwoSsrcs) {
  UnhandledPacketsBuffer buff;
  buff.AddPacket(2, *Create(3));
  buff.AddPacket(3, *Create(4));

  FakePacketReceiver receiver;
  buff.BackfillPackets({2, 3}, &receiver);
  ASSERT_EQ(2u, receiver.packets.size());
  EXPECT_EQ(*Create(3), receiver.packets[0]);
  EXPECT_EQ(*Create(4), receiver.packets[1]);
}

TEST(UnhandledPacketsBuffer, TwoPacketsTwoSsrcsOneMatch) {
  UnhandledPacketsBuffer buff;
  buff.AddPacket(2, *Create(3));
  buff.AddPacket(3, *Create(4));

  FakePacketReceiver receiver;
  buff.BackfillPackets({3}, &receiver);
  ASSERT_EQ(1u, receiver.packets.size());
  EXPECT_EQ(*Create(4), receiver.packets[0]);
}

TEST(UnhandledPacketsBuffer, Full) {
  size_t cnt = 50;
  UnhandledPacketsBuffer buff;
  for (size_t i = 0; i < cnt; i++) {
    buff.AddPacket(2, *Create(i));
  }

  FakePacketReceiver receiver;
  buff.BackfillPackets({2}, &receiver);
  ASSERT_EQ(cnt, receiver.packets.size());
  for (size_t i = 0; i < cnt; i++) {
    EXPECT_EQ(*Create(i), receiver.packets[i]);
  }
}

TEST(UnhandledPacketsBuffer, Wrap) {
  UnhandledPacketsBuffer buff;
  size_t cnt = buff.kMaxStashedPackets + 10;
  for (size_t i = 0; i < cnt; i++) {
    buff.AddPacket(2, *Create(i));
  }

  FakePacketReceiver receiver;
  buff.BackfillPackets({2}, &receiver);
  ASSERT_EQ(buff.kMaxStashedPackets, receiver.packets.size());
  for (size_t i = 0; i < receiver.packets.size(); i++) {
    EXPECT_EQ(*Create(i + 10), receiver.packets[i]);
  }
}

}  // namespace cricket
