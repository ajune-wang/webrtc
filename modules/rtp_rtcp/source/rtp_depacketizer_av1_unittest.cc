/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_depacketizer_av1.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;

TEST(RtpDepacketizerAv1Test, AssembleMinimumFrame) {
  uint8_t packet1[] = {0b00'01'0000,   // aggregation_header
                       0b0'0110'000};  // obu_header (Frame)
  rtc::ArrayView<const uint8_t> packets[] = {packet1};
  auto bitstream = RtpDepacketizerAv1::AssembleFrame(packets);
  ASSERT_TRUE(bitstream);
  // Should emit obu with payload size 0.
  EXPECT_THAT(rtc::ArrayView<const uint8_t>(*bitstream),
              ElementsAre(0b0'0110'010, 0));
}

}  // namespace
}  // namespace webrtc
