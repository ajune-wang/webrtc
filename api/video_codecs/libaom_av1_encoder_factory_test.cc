/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/libaom_av1_encoder_factory.h"

#include <utility>

#include "api/video/i420_buffer.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace {
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NotNull;
using RcMode = VideoEncoderInterface::RateControlMode;
using Cbr = VideoEncoderInterface::FrameEncodeSettings::Cbr;
using Cqp = VideoEncoderInterface::FrameEncodeSettings::Cqp;

std::unique_ptr<test::FrameReader> CreateFrameReader() {
  return CreateY4mFrameReader(
      test::ResourcePath("reference_video_640x360_30fps", "y4m"),
      test::YuvFrameReaderImpl::RepeatMode::kPingPong);
}

static constexpr VideoEncoderInterface::EncoderSettings kCbrEncoderSettings{
    .max_encode_dimensions = {.width = 1920, .height = 1080},
    .encoding_format = {.sub_sampling = EncodingFormat::SubSampling::k420,
                        .bit_depth = 8},
    .rc_mode = RcMode::kCbr,
    .max_number_of_threads = 8,
};

TEST(LibaomAv1EncoderFactory, CodecName) {
  EXPECT_THAT(LibaomAv1EncoderFactory().CodecName(), Eq("AV1"));
}

TEST(LibaomAv1EncoderFactory, CodecSpecifics) {
  EXPECT_THAT(LibaomAv1EncoderFactory().CodecSpecifics(), IsEmpty());
}

TEST(LibaomAv1EncoderFactory, CreateEncoder) {
  auto encoder =
      LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EXPECT_THAT(encoder, NotNull());
}

TEST(LibaomAv1Encoder, EncodeFrame) {
  auto frame_reader = CreateFrameReader();
  auto encoder =
      LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  bool called = false;
  VideoEncoderInterface::EncodeResultCallback callback =
      [&](const VideoEncoderInterface::EncodeResult& result) { called = true; };

  EXPECT_TRUE(encoder->Encode(
      frame_reader->PullFrame(), {},
      {{.rate_options = Cbr{.duration = TimeDelta::Millis(100),
                            .target_bitrate = DataRate::KilobitsPerSec(500)}}},
      std::move(callback)));
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace webrtc
