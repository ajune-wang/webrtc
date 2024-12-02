/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/run_until.h"

#include "api/rtc_error.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Gt;
using testing::Lt;
using testing::MatchesRegex;

TEST(RunUntilTest, ReturnsWhenConditionIsMet) {
  rtc::AutoThread thread;

  int counter = 0;
  RTCErrorOr<int> result = RunUntil([&] { return ++counter; }, Eq(3));
  EXPECT_THAT(result, IsRtcOkAndHolds(3));
}

TEST(RunUntilTest, ReturnsErrorWhenTimeoutIsReached) {
  rtc::AutoThread thread;
  int counter = 0;
  RTCErrorOr<int> result =
      RunUntil([&] { return --counter; }, Eq(1),
               {.timeout = TimeDelta::Millis(10), .result_name = "counter"});
  // Only returns the last error. Note we only are checking that the error
  // message ends with a negative number rather than a specific number to avoid
  // flakiness.
  EXPECT_THAT(
      result,
      IsRtcErrorWithMessage(
          _, MatchesRegex(
                 "Value of: counter\nExpected: is equal to 1\nActual: -\\d+")));
}

TEST(RunUntilTest, ErrorContainsMatcherExplanation) {
  rtc::AutoThread thread;
  int counter = 0;
  auto matcher = AllOf(Gt(0), Lt(10));
  RTCErrorOr<int> result =
      RunUntil([&] { return --counter; }, matcher,
               {.timeout = TimeDelta::Millis(10), .result_name = "counter"});
  // Only returns the last error. Note we only are checking that the error
  // message ends with a negative number rather than a specific number to avoid
  // flakiness.
  EXPECT_THAT(
      result,
      IsRtcErrorWithMessage(
          _, MatchesRegex("Value of: counter\nExpected: \\(is > 0\\) and "
                          "\\(is < 10\\)\nActual: -\\d+, which doesn't match "
                          "\\(is > 0\\)")));
}

}  // namespace
}  // namespace webrtc
