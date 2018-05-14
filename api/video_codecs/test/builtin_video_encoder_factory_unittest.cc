/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/builtin_video_encoder_factory.h"

#include <memory>

#include "api/video_codecs/sdp_video_format.h"
#include "test/gmock.h"

namespace {

class BuiltinVideoEncoderFactoryTest : public testing::Test {
 public:
  BuiltinVideoEncoderFactoryTest()
      : factory_(webrtc::CreateBuiltinVideoEncoderFactory()) {}

  std::unique_ptr<webrtc::VideoEncoderFactory> factory_;
};

TEST_F(BuiltinVideoEncoderFactoryTest, AnnouncesVp9AccordingToBuildFlags) {
  bool claims_vp9_support = false;
  for (const webrtc::SdpVideoFormat& format : factory_->GetSupportedFormats()) {
    if (format.name == "VP9") {
      claims_vp9_support = true;
      break;
    }
  }
#if defined(RTC_DISABLE_VP9)
  EXPECT_FALSE(claims_vp9_support);
#else
  EXPECT_TRUE(claims_vp9_support);
#endif  // defined(RTC_DISABLE_VP9)
}

}  // namespace
