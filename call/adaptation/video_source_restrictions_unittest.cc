/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
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

const size_t kHdPixels = 1280 * 720;

const VideoSourceRestrictions kUnlimited;
const VideoSourceRestrictions k15fps(absl::nullopt, absl::nullopt, 15.0);
const VideoSourceRestrictions kHd(kHdPixels, kHdPixels, absl::nullopt);
const VideoSourceRestrictions kHd15fps(kHdPixels, kHdPixels, 15.0);

VideoSourceRestrictions RestrictionsFromMaxPixelsPerFrame(
    size_t max_pixels_per_frame) {
  return VideoSourceRestrictions(max_pixels_per_frame, absl::nullopt,
                                 absl::nullopt);
}

VideoSourceRestrictions RestrictionsFromMaxFrameRate(double max_frame_rate) {
  return VideoSourceRestrictions(absl::nullopt, absl::nullopt, max_frame_rate);
}

}  // namespace

TEST(VideoSourceRestrictionsTest, DidIncreaseResolution) {
  // smaller restrictions -> larger restrictions
  EXPECT_TRUE(DidIncreaseResolution(RestrictionsFromMaxPixelsPerFrame(10),
                                    RestrictionsFromMaxPixelsPerFrame(11)));
  // unrestricted -> restricted
  EXPECT_FALSE(DidIncreaseResolution(VideoSourceRestrictions(),
                                     RestrictionsFromMaxPixelsPerFrame(10)));
  // restricted -> unrestricted
  EXPECT_TRUE(DidIncreaseResolution(RestrictionsFromMaxPixelsPerFrame(10),
                                    VideoSourceRestrictions()));
  // restricted -> equally restricted
  EXPECT_FALSE(DidIncreaseResolution(RestrictionsFromMaxPixelsPerFrame(10),
                                     RestrictionsFromMaxPixelsPerFrame(10)));
  // unrestricted -> unrestricted
  EXPECT_FALSE(DidIncreaseResolution(VideoSourceRestrictions(),
                                     VideoSourceRestrictions()));
  // larger restrictions -> smaller restrictions
  EXPECT_FALSE(DidIncreaseResolution(RestrictionsFromMaxPixelsPerFrame(10),
                                     RestrictionsFromMaxPixelsPerFrame(9)));
}

TEST(VideoSourceRestrictionsTest, DidDecreaseFrameRate) {
  // samller restrictions -> larger restrictions
  EXPECT_FALSE(DidDecreaseFrameRate(RestrictionsFromMaxFrameRate(10),
                                    RestrictionsFromMaxFrameRate(11)));
  // unrestricted -> restricted
  EXPECT_TRUE(DidDecreaseFrameRate(VideoSourceRestrictions(),
                                   RestrictionsFromMaxFrameRate(10)));
  // restricted -> unrestricted
  EXPECT_FALSE(DidDecreaseFrameRate(RestrictionsFromMaxFrameRate(10),
                                    VideoSourceRestrictions()));
  // restricted -> equally restricted
  EXPECT_FALSE(DidDecreaseFrameRate(RestrictionsFromMaxFrameRate(10),
                                    RestrictionsFromMaxFrameRate(10)));
  // unrestricted -> unrestricted
  EXPECT_FALSE(DidDecreaseFrameRate(VideoSourceRestrictions(),
                                    VideoSourceRestrictions()));
  // larger restrictions -> samller restrictions
  EXPECT_TRUE(DidDecreaseFrameRate(RestrictionsFromMaxFrameRate(10),
                                   RestrictionsFromMaxFrameRate(9)));
}

TEST(VideoSourceRestrictionsTest, DidRestrictionsChangeFalseForSame) {
  EXPECT_FALSE(DidRestrictionsDecrease(kUnlimited, kUnlimited));
  EXPECT_FALSE(DidRestrictionsIncrease(kUnlimited, kUnlimited));
}

TEST(VideoSourceRestrictions,
     DidRestrictionsIncreaseTrueWhenPixelsOrFrameRateDecreased) {
  EXPECT_TRUE(DidRestrictionsIncrease(kUnlimited, kHd));
  EXPECT_TRUE(DidRestrictionsIncrease(kUnlimited, k15fps));
  EXPECT_TRUE(DidRestrictionsIncrease(kHd, kHd15fps));
  EXPECT_TRUE(DidRestrictionsIncrease(kUnlimited, kHd15fps));
}

TEST(VideoSourceRestrictions,
     DidRestrictionsDecreaseTrueWhenPixelsOrFrameRateIncreased) {
  EXPECT_TRUE(DidRestrictionsDecrease(kHd, kUnlimited));
  EXPECT_TRUE(DidRestrictionsDecrease(k15fps, kUnlimited));
  EXPECT_TRUE(DidRestrictionsDecrease(kHd15fps, kHd));
  EXPECT_TRUE(DidRestrictionsDecrease(kHd15fps, kUnlimited));
}

TEST(VideoSourceRestrictions,
     DidRestrictionsChangeFalseWhenFrameRateAndPixelsChangeDifferently) {
  // One changed framerate, the other resolution; not an increase or decrease.
  EXPECT_FALSE(DidRestrictionsIncrease(kHd, k15fps));
  EXPECT_FALSE(DidRestrictionsDecrease(kHd, k15fps));
}

}  // namespace webrtc
