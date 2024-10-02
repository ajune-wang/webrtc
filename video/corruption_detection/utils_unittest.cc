/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/utils.h"

#include "api/video/video_codec_type.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;

TEST(UtilsTest, FindCodecFromString) {
  EXPECT_EQ(GetVideoCodecType(/*codec_name=*/"VP8"), kVideoCodecVP8);
  EXPECT_EQ(GetVideoCodecType(/*codec_name=*/"libvpx-vp9"), kVideoCodecVP9);
  EXPECT_EQ(GetVideoCodecType(/*codec_name=*/"ImprovedAV1"), kVideoCodecAV1);
  EXPECT_EQ(GetVideoCodecType(/*codec_name=*/"lets_use_h264"), kVideoCodecH264);
}

TEST(UtilsTest, IfCodecNotRaiseError) {
  EXPECT_DEATH(GetVideoCodecType(/*codec_name=*/"Not_a_codec"), _);
}

}  // namespace
}  // namespace webrtc
