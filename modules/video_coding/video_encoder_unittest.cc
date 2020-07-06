/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/video_encoder.h"
#include "api/video_codecs/video_codec.h"
#include "modules/video_coding/video_receiver2.h"
#include "test/gtest.h"

namespace webrtc {
namespace webrtc_internal {
namespace {

TEST(VideoEncoderTest, Something) {
  VideoCodec video_codec;
  video_codec.active = true;
  VideoEncoder encoder;
  EXPECT_TRUE(encoder.Configure(video_codec));
}

}  // namespace
}  // namespace webrtc_internal
}  // namespace webrtc
