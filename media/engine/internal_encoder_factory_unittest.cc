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
#ifdef RTC_ENABLE_VP9
constexpr bool kVp9Enabled = true;
#else
constexpr bool kVp9Enabled = false;
#endif
constexpr VideoEncoderFactory::CodecSupport kSupported = {true, false};
constexpr VideoEncoderFactory::CodecSupport kUnsupported = {false, false};

bool Equals(VideoEncoderFactory::CodecSupport a,
            VideoEncoderFactory::CodecSupport b) {
  return a.is_supported == b.is_supported &&
         a.is_power_efficient == b.is_power_efficient;
}
}  // namespace

using ::testing::Contains;
using ::testing::Field;
using ::testing::Not;

TEST(InternalEncoderFactory, TestVP8) {
  InternalEncoderFactory factory;
  std::unique_ptr<VideoEncoder> encoder =
      factory.CreateVideoEncoder(SdpVideoFormat(cricket::kVp8CodecName));
  EXPECT_TRUE(encoder);
}

TEST(InternalEncoderFactory, TestVP9Profile0) {
  InternalEncoderFactory factory;
  std::unique_ptr<VideoEncoder> encoder =
      factory.CreateVideoEncoder(SdpVideoFormat(
          cricket::kVp9CodecName,
          {{kVP9FmtpProfileId, VP9ProfileToString(VP9Profile::kProfile0)}}));
  EXPECT_EQ(static_cast<bool>(encoder), kVp9Enabled);
}

TEST(InternalEncoderFactory, TestVP9Profile1) {
  InternalEncoderFactory factory;
  std::unique_ptr<VideoEncoder> encoder =
      factory.CreateVideoEncoder(SdpVideoFormat(
          cricket::kVp9CodecName,
          {{kVP9FmtpProfileId, VP9ProfileToString(VP9Profile::kProfile1)}}));
  EXPECT_EQ(static_cast<bool>(encoder), kVp9Enabled);
}

TEST(InternalEncoderFactory, Av1) {
  InternalEncoderFactory factory;
  if (kIsLibaomAv1EncoderSupported) {
    EXPECT_THAT(factory.GetSupportedFormats(),
                Contains(Field(&SdpVideoFormat::name, "AV1X")));
    EXPECT_TRUE(factory.CreateVideoEncoder(SdpVideoFormat("AV1X")));
  } else {
    EXPECT_THAT(factory.GetSupportedFormats(),
                Not(Contains(Field(&SdpVideoFormat::name, "AV1X"))));
  }
}

TEST(InternalEncoderFactory, QueryCodecSupportNoSvc) {
  InternalEncoderFactory factory;
  EXPECT_TRUE(
      Equals(factory.QueryCodecSupport(SdpVideoFormat("VP8"),
                                       /*scalability_mode=*/absl::nullopt),
             kSupported));
  EXPECT_TRUE(
      Equals(factory.QueryCodecSupport(SdpVideoFormat("VP9"),
                                       /*scalability_mode=*/absl::nullopt),
             kVp9Enabled ? kSupported : kUnsupported));
  EXPECT_TRUE(Equals(
      factory.QueryCodecSupport(
          SdpVideoFormat("VP9", {{kVP9FmtpProfileId,
                                  VP9ProfileToString(VP9Profile::kProfile2)}}),
          /*scalability_mode=*/absl::nullopt),
      kVp9Enabled ? kSupported : kUnsupported));
  EXPECT_TRUE(
      Equals(factory.QueryCodecSupport(SdpVideoFormat("AV1X"),
                                       /*scalability_mode=*/absl::nullopt),
             kIsLibaomAv1EncoderSupported ? kSupported : kUnsupported));
}

TEST(InternalEncoderFactory, QueryCodecSupportSvc) {
  InternalEncoderFactory factory;
  // VP8 and VP9 supported for singles spatial layers.
  EXPECT_TRUE(Equals(factory.QueryCodecSupport(SdpVideoFormat("VP8"), "L1T2"),
                     kSupported));
  EXPECT_TRUE(Equals(factory.QueryCodecSupport(SdpVideoFormat("VP9"), "L1T3"),
                     kVp9Enabled ? kSupported : kUnsupported));

  // VP9 support for spatial layers.
  EXPECT_TRUE(Equals(factory.QueryCodecSupport(SdpVideoFormat("VP9"), "L3T3"),
                     kVp9Enabled ? kSupported : kUnsupported));

  EXPECT_TRUE(Equals(factory.QueryCodecSupport(SdpVideoFormat("AV1X"), "L2T1"),
                     kIsLibaomAv1EncoderSupported ? kSupported : kUnsupported));

  // Invalid SVC config even though VP8 is supported.
  EXPECT_TRUE(Equals(factory.QueryCodecSupport(SdpVideoFormat("H264"), "L1T2"),
                     kUnsupported));
  EXPECT_TRUE(Equals(factory.QueryCodecSupport(SdpVideoFormat("VP8"), "L3T3"),
                     kUnsupported));
}

}  // namespace webrtc
