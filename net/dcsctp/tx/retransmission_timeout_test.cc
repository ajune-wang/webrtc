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

#include "api/units/time_delta.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::webrtc::TimeDelta;

constexpr webrtc::TimeDelta kMaxRtt = webrtc::TimeDelta::Seconds(8);
constexpr webrtc::TimeDelta kInitialRto = webrtc::TimeDelta::Millis(200);
constexpr webrtc::TimeDelta kMaxRto = webrtc::TimeDelta::Millis(800);
constexpr webrtc::TimeDelta kMinRto = webrtc::TimeDelta::Millis(120);

DcSctpOptions MakeOptions() {
  DcSctpOptions options;
  options.rtt_max = DurationMs(kMaxRtt.ms());
  options.rto_initial = DurationMs(kInitialRto.ms());
  options.rto_max = DurationMs(kMaxRto.ms());
  options.rto_min = DurationMs(kMinRto.ms());
  return options;
}

TEST(RetransmissionTimeoutTest, HasValidInitialRto) {
  RetransmissionTimeout rto_(MakeOptions());
  EXPECT_EQ(rto_.rto(), kInitialRto);
}

TEST(RetransmissionTimeoutTest, NegativeValuesDoNotAffectRTO) {
  RetransmissionTimeout rto_(MakeOptions());
  // Initial negative value
  rto_.ObserveRTT(webrtc::TimeDelta::Millis(-10));
  EXPECT_EQ(rto_.rto(), kInitialRto);
  rto_.ObserveRTT(webrtc::TimeDelta::Millis(124));
  EXPECT_EQ(rto_.rto().ms(), 372);
  // Subsequent negative value
  rto_.ObserveRTT(webrtc::TimeDelta::Millis(-10));
  EXPECT_EQ(rto_.rto().ms(), 372);
}

TEST(RetransmissionTimeoutTest, TooLargeValuesDoNotAffectRTO) {
  RetransmissionTimeout rto_(MakeOptions());
  // Initial too large value
  rto_.ObserveRTT(kMaxRtt + webrtc::TimeDelta::Millis(100));
  EXPECT_EQ(rto_.rto(), kInitialRto);
  rto_.ObserveRTT(webrtc::TimeDelta::Millis(124));
  EXPECT_EQ(rto_.rto().ms(), 372);
  // Subsequent too large value
  rto_.ObserveRTT(kMaxRtt + webrtc::TimeDelta::Millis(100));
  EXPECT_EQ(rto_.rto().ms(), 372);
}

TEST(RetransmissionTimeoutTest, WillNeverGoBelowMinimumRto) {
  RetransmissionTimeout rto_(MakeOptions());
  for (int i = 0; i < 1000; ++i) {
    rto_.ObserveRTT(webrtc::TimeDelta::Millis(1));
  }
  EXPECT_GE(rto_.rto(), kMinRto);
}

TEST(RetransmissionTimeoutTest, WillNeverGoAboveMaximumRto) {
  RetransmissionTimeout rto_(MakeOptions());
  for (int i = 0; i < 1000; ++i) {
    rto_.ObserveRTT(kMaxRtt - webrtc::TimeDelta::Millis(1));
    // Adding jitter, which would make it RTO be well above RTT.
    rto_.ObserveRTT(kMaxRtt - webrtc::TimeDelta::Millis(100));
  }
  EXPECT_LE(rto_.rto(), kMaxRto);
}

TEST(RetransmissionTimeoutTest, CalculatesRtoForStableRtt) {
  RetransmissionTimeout rto_(MakeOptions());
  rto_.ObserveRTT(TimeDelta::Millis(124));
  EXPECT_EQ(rto_.rto().ms(), 372);
  rto_.ObserveRTT(TimeDelta::Millis(128));
  EXPECT_EQ(rto_.rto().ms(), 314);
  rto_.ObserveRTT(TimeDelta::Millis(123));
  EXPECT_EQ(rto_.rto().ms(), 268);
  rto_.ObserveRTT(TimeDelta::Millis(125));
  EXPECT_EQ(rto_.rto().ms(), 233);
  rto_.ObserveRTT(TimeDelta::Millis(127));
  EXPECT_EQ(rto_.rto().ms(), 209);
}

TEST(RetransmissionTimeoutTest, CalculatesRtoForUnstableRtt) {
  RetransmissionTimeout rto_(MakeOptions());
  rto_.ObserveRTT(webrtc::TimeDelta::Millis(124));
  EXPECT_EQ(rto_.rto().ms(), 372);
  rto_.ObserveRTT(webrtc::TimeDelta::Millis(402));
  EXPECT_EQ(rto_.rto().ms(), 622);
  rto_.ObserveRTT(webrtc::TimeDelta::Millis(728));
  EXPECT_EQ(rto_.rto().ms(), 800);
  rto_.ObserveRTT(webrtc::TimeDelta::Millis(89));
  EXPECT_EQ(rto_.rto().ms(), 800);
  rto_.ObserveRTT(webrtc::TimeDelta::Millis(126));
  EXPECT_EQ(rto_.rto().ms(), 800);
}

TEST(RetransmissionTimeoutTest, WillStabilizeAfterAWhile) {
  RetransmissionTimeout rto_(MakeOptions());
  rto_.ObserveRTT(TimeDelta::Millis(124));
  rto_.ObserveRTT(TimeDelta::Millis(402));
  rto_.ObserveRTT(TimeDelta::Millis(728));
  rto_.ObserveRTT(TimeDelta::Millis(89));
  rto_.ObserveRTT(TimeDelta::Millis(126));
  EXPECT_EQ(rto_.rto().ms(), 800);
  rto_.ObserveRTT(TimeDelta::Millis(124));
  EXPECT_EQ(rto_.rto().ms(), 800);
  rto_.ObserveRTT(TimeDelta::Millis(122));
  EXPECT_EQ(rto_.rto().ms(), 710);
  rto_.ObserveRTT(TimeDelta::Millis(123));
  EXPECT_EQ(rto_.rto().ms(), 631);
  rto_.ObserveRTT(TimeDelta::Millis(124));
  EXPECT_EQ(rto_.rto().ms(), 562);
  rto_.ObserveRTT(TimeDelta::Millis(122));
  EXPECT_EQ(rto_.rto().ms(), 505);
  rto_.ObserveRTT(TimeDelta::Millis(124));
  EXPECT_EQ(rto_.rto().ms(), 454);
  rto_.ObserveRTT(TimeDelta::Millis(124));
  EXPECT_EQ(rto_.rto().ms(), 410);
  rto_.ObserveRTT(TimeDelta::Millis(124));
  EXPECT_EQ(rto_.rto().ms(), 372);
  rto_.ObserveRTT(TimeDelta::Millis(124));
  EXPECT_EQ(rto_.rto().ms(), 340);
}

TEST(RetransmissionTimeoutTest, WillAlwaysStayAboveRTT) {
  // In simulations, it's quite common to have a very stable RTT, and having an
  // RTO at the same value will cause issues as expiry timers will be scheduled
  // to be expire exactly when a packet is supposed to arrive. The RTO must be
  // larger than the RTT. In non-simulated environments, this is a non-issue as
  // any jitter will increase the RTO.
  RetransmissionTimeout rto_(MakeOptions());

  for (int i = 0; i < 100; ++i) {
    rto_.ObserveRTT(webrtc::TimeDelta::Millis(124));
  }
  EXPECT_GT(rto_.rto().ms(), 124);
}

}  // namespace
}  // namespace dcsctp
