/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/receive_time_calculator.h"

#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {
int64_t kDefaultMinDeltaDiffMs = -100;
int64_t kDefaultMaxDeltaDiffMs = 100;
int64_t kPacketIncrementUs = 900;
}  // namespace

TEST(ReceiveTimeCalculatorTest, UsesSmallerIncrements) {
  ReceiveTimeCalculator calc(kDefaultMinDeltaDiffMs, kDefaultMaxDeltaDiffMs);
  int64_t packet_clock_us = 1000000;
  int64_t safe_clock_us = 4000000;
  int64_t true_clock_us = safe_clock_us;
  int packet_group_size = 10;

  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < packet_group_size; ++j) {
      int64_t reconciled_us =
          calc.ReconcileReceiveTimes(packet_clock_us, safe_clock_us);
      EXPECT_EQ(reconciled_us, true_clock_us);
      true_clock_us += kPacketIncrementUs;
      packet_clock_us += kPacketIncrementUs;
    }
    safe_clock_us += packet_group_size * kPacketIncrementUs;
  }
}

TEST(ReceiveTimeCalculatorTest, CorrectsJumps) {
  ReceiveTimeCalculator calc(kDefaultMinDeltaDiffMs, kDefaultMaxDeltaDiffMs);
  int64_t packet_clock_us = 1000000;
  int64_t safe_clock_us = 4000000;
  int64_t true_clock_us = safe_clock_us;
  int packet_group_size = 10;

  for (int i = 0; i < 5; ++i) {
    for (int j = 0; j < packet_group_size; ++j) {
      int64_t reconciled_us =
          calc.ReconcileReceiveTimes(packet_clock_us, safe_clock_us);
      EXPECT_EQ(reconciled_us, true_clock_us);
      true_clock_us += kPacketIncrementUs;
      packet_clock_us += kPacketIncrementUs;
    }
    safe_clock_us += packet_group_size * kPacketIncrementUs;
  }
  packet_clock_us += kDefaultMinDeltaDiffMs * 1000 + 1;

  for (int i = 0; i < 5; ++i) {
    for (int j = 0; j < packet_group_size; ++j) {
      int64_t reconciled_us =
          calc.ReconcileReceiveTimes(packet_clock_us, safe_clock_us);
      EXPECT_EQ(reconciled_us, true_clock_us);
      true_clock_us += kPacketIncrementUs;
      packet_clock_us += kPacketIncrementUs;
    }
    safe_clock_us += packet_group_size * kPacketIncrementUs;
  }
  packet_clock_us += kDefaultMinDeltaDiffMs * 1000 - 1;

  for (int i = 0; i < 5; ++i) {
    for (int j = 0; j < packet_group_size; ++j) {
      int64_t reconciled_us =
          calc.ReconcileReceiveTimes(packet_clock_us, safe_clock_us);
      EXPECT_EQ(reconciled_us, true_clock_us);
      true_clock_us += kPacketIncrementUs;
      packet_clock_us += kPacketIncrementUs;
    }
    safe_clock_us += packet_group_size * kPacketIncrementUs;
  }
}

}  // namespace test

}  // namespace webrtc
