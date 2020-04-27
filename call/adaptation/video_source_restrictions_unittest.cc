/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/video_source_restrictions.h"

#include "test/gtest.h"

namespace webrtc {

namespace {

const size_t kHd = 1280 * 720;
const size_t kVga = 640 * 480;

}  // namespace

TEST(VideoSourceRestrictionsTest, Lt) {
  VideoSourceRestrictions unlimited;
  VideoSourceRestrictions hd(kHd, kHd, absl::nullopt);
  VideoSourceRestrictions hd_15fps(kHd, kHd, 15.0);
  VideoSourceRestrictions vga_15fps(kVga, kVga, 15.0);
  VideoSourceRestrictions vga_7fps(kVga, kVga, 7.0);

  EXPECT_LT(hd, unlimited);
  EXPECT_LT(hd_15fps, hd);
  EXPECT_LT(vga_15fps, hd_15fps);
  EXPECT_LT(vga_7fps, vga_15fps);
}

}  // namespace webrtc
