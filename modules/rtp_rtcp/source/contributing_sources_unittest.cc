/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/contributing_sources.h"

#include "rtc_base/timeutils.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::UnorderedElementsAre;

const uint32_t kCsrc1 = 111;
const uint32_t kCsrc2 = 222;
const uint32_t kCsrc3 = 333;

}  // namespace

TEST(ContributingSourcesTest, GetSources) {
  ContributingSources csrcs;
  const uint32_t kCsrcs[] = { kCsrc1, kCsrc2 };
  const int64_t kTime1 = 10;
  csrcs.Update(kTime1, kCsrcs);
  EXPECT_THAT(csrcs.GetSources(kTime1), UnorderedElementsAre(
                           RtpSource(kTime1, kCsrc1, RtpSourceType::CSRC),
                           RtpSource(kTime1, kCsrc2, RtpSourceType::CSRC)));
}

TEST(ContributingSourcesTest, UpdateSources) {
  ContributingSources csrcs;
  // TODO(nisse): This is clumsy, any nice way to constuct an array
  // view with in in-place array literal?
  const uint32_t kCsrcs1[] = { kCsrc1, kCsrc2 };
  const uint32_t kCsrcs2[] = { kCsrc3 };
  const int64_t kTime1 = 10;
  const int64_t kTime2 = kTime1 + 5 * rtc::kNumMillisecsPerSec;
  const int64_t kTime3 = kTime1 + 12 * rtc::kNumMillisecsPerSec;
  csrcs.Update(kTime1, kCsrcs1);
  EXPECT_THAT(csrcs.GetSources(kTime1), UnorderedElementsAre(
                           RtpSource(kTime1, kCsrc1, RtpSourceType::CSRC),
                           RtpSource(kTime1, kCsrc2, RtpSourceType::CSRC)));
  csrcs.Update(kTime2, kCsrcs2);
  EXPECT_THAT(csrcs.GetSources(kTime2), UnorderedElementsAre(
                           RtpSource(kTime1, kCsrc1, RtpSourceType::CSRC),
                           RtpSource(kTime1, kCsrc2, RtpSourceType::CSRC),
                           RtpSource(kTime2, kCsrc3, RtpSourceType::CSRC)));
  EXPECT_THAT(csrcs.GetSources(kTime3), UnorderedElementsAre(
                           RtpSource(kTime2, kCsrc3, RtpSourceType::CSRC)));
}

}  // namespace webrtc
