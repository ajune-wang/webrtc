/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "common_types.h"  // NOLINT(build/include)
#include "media/base/mediaconstants.h"
#if defined(WEBRTC_ANDROID)
#include "modules/video_coding/codecs/test/android_codec_factory_helper.h"
#elif defined(WEBRTC_IOS)
#include "modules/video_coding/codecs/test/objc_codec_factory_helper.h"
#endif
#include "test/gtest.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace test {

namespace {

int32_t InitEncoder(VideoCodecType codec_type, VideoEncoder* encoder) {
  VideoCodec codec;
  CodecSettings(codec_type, &codec);
  codec.width = 1280;
  codec.height = 720;
  codec.maxFramerate = 30;
  RTC_CHECK(encoder);
  return encoder->InitEncode(&codec, 1 /* number_of_cores */,
                             1200 /* max_payload_size */);
}

}  // namespace

class VideoEncoderInstantiationTest
    : public testing::Test,
      public ::testing::WithParamInterface<int> {
 protected:
  VideoEncoderInstantiationTest()
      : num_encoders_(GetParam()),
        vp8_format_(cricket::kVp8CodecName),
        vp9_format_(cricket::kVp9CodecName),
        h264cbp_format_(cricket::kH264CodecName) {
#if defined(WEBRTC_ANDROID)
    InitializeAndroidObjects();
    encoder_factory_ = CreateAndroidEncoderFactory();
#elif defined(WEBRTC_ANDROID)
    encoder_factory_ = CreateObjCEncoderFactory();
#else
    RTC_NOTREACHED() << "Only support Android and iOS.";
#endif
  }

  ~VideoEncoderInstantiationTest() {
    for (auto& encoder : encoders_) {
      encoder->Release();
    }
  }

  const int num_encoders_;
  const SdpVideoFormat vp8_format_;
  const SdpVideoFormat vp9_format_;
  const SdpVideoFormat h264cbp_format_;
  std::unique_ptr<VideoEncoderFactory> encoder_factory_;

  std::vector<std::unique_ptr<VideoEncoder>> encoders_;
};

INSTANTIATE_TEST_CASE_P(MultipleEncoders,
                        VideoEncoderInstantiationTest,
                        ::testing::Range(1, 9));

TEST_P(VideoEncoderInstantiationTest, InstantiateNVp8Encoders) {
  for (int i = 0; i < num_encoders_; ++i) {
    std::unique_ptr<VideoEncoder> encoder =
        encoder_factory_->CreateVideoEncoder(vp8_format_);
    EXPECT_EQ(0, InitEncoder(kVideoCodecVP8, encoder.get()));
    encoders_.emplace_back(std::move(encoder));
  }
}

TEST_P(VideoEncoderInstantiationTest, InstantiateNH264CBPEncoders) {
  for (int i = 0; i < num_encoders_; ++i) {
    std::unique_ptr<VideoEncoder> encoder =
        encoder_factory_->CreateVideoEncoder(h264cbp_format_);
    EXPECT_EQ(0, InitEncoder(kVideoCodecH264, encoder.get()));
    encoders_.emplace_back(std::move(encoder));
  }
}

}  // namespace test
}  // namespace webrtc
