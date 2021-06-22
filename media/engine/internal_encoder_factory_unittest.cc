/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/internal_encoder_factory.h"

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp9_profile.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using ::testing::Contains;
using ::testing::Field;
using ::testing::Not;

#ifdef RTC_ENABLE_VP9
constexpr bool kVp9Enabled = true;
#else
constexpr bool kVp9Enabled = false;
#endif
#ifdef WEBRTC_USE_H264
constexpr bool kH264Enabled = true;
#else
constexpr bool kH264Enabled = false;
#endif
constexpr VideoEncoderFactory::CodecSupport kSupported = {true, false};
constexpr VideoEncoderFactory::CodecSupport kUnsupported = {false, false};

bool Equals(VideoEncoderFactory::CodecSupport a,
            VideoEncoderFactory::CodecSupport b) {
  return a.is_supported == b.is_supported &&
         a.is_power_efficient == b.is_power_efficient;
}

TEST(InternalEncoderFactoryTest, Vp8) {
  InternalEncoderFactory factory;
  std::unique_ptr<VideoEncoder> encoder =
      factory.CreateVideoEncoder(SdpVideoFormat(cricket::kVp8CodecName));
  EXPECT_TRUE(encoder);
}

TEST(InternalEncoderFactoryTest, Vp9Profile0) {
  InternalEncoderFactory factory;
  std::unique_ptr<VideoEncoder> encoder =
      factory.CreateVideoEncoder(SdpVideoFormat(
          cricket::kVp9CodecName,
          {{kVP9FmtpProfileId, VP9ProfileToString(VP9Profile::kProfile0)}}));
  EXPECT_EQ(static_cast<bool>(encoder), kVp9Enabled);
}

TEST(InternalEncoderFactoryTest, H264) {
  InternalEncoderFactory factory;
  std::unique_ptr<VideoEncoder> encoder =
      factory.CreateVideoEncoder(SdpVideoFormat(cricket::kH264CodecName));
  EXPECT_EQ(static_cast<bool>(encoder), kH264Enabled);
}

TEST(InternalEncoderFactoryTest, Av1) {
  InternalEncoderFactory factory;
  if (kIsLibaomAv1EncoderSupported) {
    EXPECT_THAT(factory.GetSupportedFormats(),
                Contains(Field(&SdpVideoFormat::name, cricket::kAv1CodecName)));
    EXPECT_TRUE(
        factory.CreateVideoEncoder(SdpVideoFormat(cricket::kAv1CodecName)));
  } else {
    EXPECT_THAT(
        factory.GetSupportedFormats(),
        Not(Contains(Field(&SdpVideoFormat::name, cricket::kAv1CodecName))));
  }
}

TEST(InternalEncoderFactoryTest, QueryCodecSupportNoSvc) {
  InternalEncoderFactory factory;
  EXPECT_TRUE(
      Equals(factory.QueryCodecSupport(SdpVideoFormat(cricket::kVp8CodecName),
                                       /*scalability_mode=*/absl::nullopt),
             kSupported));
  EXPECT_TRUE(
      Equals(factory.QueryCodecSupport(SdpVideoFormat(cricket::kVp9CodecName),
                                       /*scalability_mode=*/absl::nullopt),
             kVp9Enabled ? kSupported : kUnsupported));
  EXPECT_TRUE(Equals(
      factory.QueryCodecSupport(
          SdpVideoFormat(
              cricket::kVp9CodecName,
              {{kVP9FmtpProfileId, VP9ProfileToString(VP9Profile::kProfile2)}}),
          /*scalability_mode=*/absl::nullopt),
      kVp9Enabled ? kSupported : kUnsupported));
  EXPECT_TRUE(
      Equals(factory.QueryCodecSupport(SdpVideoFormat(cricket::kAv1CodecName),
                                       /*scalability_mode=*/absl::nullopt),
             kIsLibaomAv1EncoderSupported ? kSupported : kUnsupported));
}

TEST(InternalEncoderFactoryTest, QueryCodecSupportSvc) {
  InternalEncoderFactory factory;
  // VP8 and VP9 supported for singles spatial layers.
  EXPECT_TRUE(Equals(
      factory.QueryCodecSupport(SdpVideoFormat(cricket::kVp8CodecName), "L1T2"),
      kSupported));
  EXPECT_TRUE(Equals(
      factory.QueryCodecSupport(SdpVideoFormat(cricket::kVp9CodecName), "L1T3"),
      kVp9Enabled ? kSupported : kUnsupported));

  // VP9 support for spatial layers.
  EXPECT_TRUE(Equals(
      factory.QueryCodecSupport(SdpVideoFormat(cricket::kVp9CodecName), "L3T3"),
      kVp9Enabled ? kSupported : kUnsupported));

  EXPECT_TRUE(Equals(
      factory.QueryCodecSupport(SdpVideoFormat(cricket::kAv1CodecName), "L2T1"),
      kIsLibaomAv1EncoderSupported ? kSupported : kUnsupported));

  // Invalid SVC config even though VP8 and H264 are supported.
  EXPECT_TRUE(Equals(factory.QueryCodecSupport(
                         SdpVideoFormat(cricket::kH264CodecName), "L1T2"),
                     kUnsupported));
  EXPECT_TRUE(Equals(
      factory.QueryCodecSupport(SdpVideoFormat(cricket::kVp8CodecName), "L3T3"),
      kUnsupported));
}

}  // namespace
}  // namespace webrtc
