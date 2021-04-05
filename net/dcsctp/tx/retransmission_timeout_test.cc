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
  EXPECT_EQ(rto_.rto(), options.rto_initial);
}

TEST(RetransmissionTimeoutTest, WillNeverGoBelowMinimumRto) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  for (int i = 0; i < 1000; ++i) {
    rto_.ObserveRTT(DurationMs(1));
  }
  EXPECT_GE(rto_.rto(), options.rto_min);
}

TEST(RetransmissionTimeoutTest, WillNeverGoAboveMaximumRto) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  for (int i = 0; i < 1000; ++i) {
    rto_.ObserveRTT(DurationMs(3'600'000));
  }
  EXPECT_LE(rto_.rto(), options.rto_max);
}

TEST(RetransmissionTimeoutTest, CalculatesRtoForStableRtt) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(rto_.rto(), DurationMs(372));
  rto_.ObserveRTT(DurationMs(128));
  EXPECT_THAT(rto_.rto(), DurationMs(312));
  rto_.ObserveRTT(DurationMs(123));
  EXPECT_THAT(rto_.rto(), DurationMs(263));
  rto_.ObserveRTT(DurationMs(125));
  EXPECT_THAT(rto_.rto(), DurationMs(227));
  rto_.ObserveRTT(DurationMs(127));
  EXPECT_THAT(rto_.rto(), DurationMs(203));
}

TEST(RetransmissionTimeoutTest, CalculatesRtoForUnstableRtt) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(rto_.rto(), DurationMs(372));
  rto_.ObserveRTT(DurationMs(402));
  EXPECT_THAT(rto_.rto(), DurationMs(622));
  rto_.ObserveRTT(DurationMs(728));
  EXPECT_THAT(rto_.rto(), DurationMs(800));
  rto_.ObserveRTT(DurationMs(89));
  EXPECT_THAT(rto_.rto(), DurationMs(800));
  rto_.ObserveRTT(DurationMs(126));
  EXPECT_THAT(rto_.rto(), DurationMs(800));
}

TEST(RetransmissionTimeoutTest, WillStabilizeAfterAWhile) {
  DcSctpOptions options;

  RetransmissionTimeout rto_(options);
  rto_.ObserveRTT(DurationMs(124));
  rto_.ObserveRTT(DurationMs(402));
  rto_.ObserveRTT(DurationMs(728));
  rto_.ObserveRTT(DurationMs(89));
  rto_.ObserveRTT(DurationMs(126));
  EXPECT_THAT(rto_.rto(), DurationMs(800));
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(rto_.rto(), DurationMs(790));
  rto_.ObserveRTT(DurationMs(122));
  EXPECT_THAT(rto_.rto(), DurationMs(697));
  rto_.ObserveRTT(DurationMs(123));
  EXPECT_THAT(rto_.rto(), DurationMs(617));
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(rto_.rto(), DurationMs(546));
  rto_.ObserveRTT(DurationMs(122));
  EXPECT_THAT(rto_.rto(), DurationMs(488));
  rto_.ObserveRTT(DurationMs(124));
  EXPECT_THAT(rto_.rto(), DurationMs(435));
}
}  // namespace
}  // namespace dcsctp
