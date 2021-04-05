/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/retransmission_timeout.h"

#include "net/dcsctp/public/dcsctp_options.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {

TEST(RetransmissionTimeoutTest, HasValidInitialRto) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  EXPECT_EQ(rto_.rto_ms(), options.rto_initial_ms);
}

TEST(RetransmissionTimeoutTest, WillNeverGoBelowMinimumRto) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  for (int i = 0; i < 1000; ++i) {
    rto_.ObserveRTT(1);
  }
  EXPECT_GE(rto_.rto_ms(), options.rto_min_ms);
}

TEST(RetransmissionTimeoutTest, WillNeverGoAboveMaximumRto) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  for (int i = 0; i < 1000; ++i) {
    rto_.ObserveRTT(3'600'000);
  }
  EXPECT_LE(rto_.rto_ms(), options.rto_max_ms);
}

TEST(RetransmissionTimeoutTest, CalculatesRtoForStableRtt) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  rto_.ObserveRTT(124);
  EXPECT_THAT(rto_.rto_ms(), 372);
  rto_.ObserveRTT(128);
  EXPECT_THAT(rto_.rto_ms(), 312);
  rto_.ObserveRTT(123);
  EXPECT_THAT(rto_.rto_ms(), 263);
  rto_.ObserveRTT(125);
  EXPECT_THAT(rto_.rto_ms(), 227);
  rto_.ObserveRTT(127);
  EXPECT_THAT(rto_.rto_ms(), 203);
}

TEST(RetransmissionTimeoutTest, CalculatesRtoForUnstableRtt) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  rto_.ObserveRTT(124);
  EXPECT_THAT(rto_.rto_ms(), 372);
  rto_.ObserveRTT(402);
  EXPECT_THAT(rto_.rto_ms(), 622);
  rto_.ObserveRTT(728);
  EXPECT_THAT(rto_.rto_ms(), 800);
  rto_.ObserveRTT(89);
  EXPECT_THAT(rto_.rto_ms(), 800);
  rto_.ObserveRTT(126);
  EXPECT_THAT(rto_.rto_ms(), 800);
}

TEST(RetransmissionTimeoutTest, WillStabilizeAfterAWhile) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  rto_.ObserveRTT(124);
  rto_.ObserveRTT(402);
  rto_.ObserveRTT(728);
  rto_.ObserveRTT(89);
  rto_.ObserveRTT(126);
  EXPECT_THAT(rto_.rto_ms(), 800);
  rto_.ObserveRTT(124);
  EXPECT_THAT(rto_.rto_ms(), 790);
  rto_.ObserveRTT(122);
  EXPECT_THAT(rto_.rto_ms(), 697);
  rto_.ObserveRTT(123);
  EXPECT_THAT(rto_.rto_ms(), 617);
  rto_.ObserveRTT(124);
  EXPECT_THAT(rto_.rto_ms(), 546);
  rto_.ObserveRTT(122);
  EXPECT_THAT(rto_.rto_ms(), 488);
  rto_.ObserveRTT(124);
  EXPECT_THAT(rto_.rto_ms(), 435);
}
}  // namespace
}  // namespace dcsctp
