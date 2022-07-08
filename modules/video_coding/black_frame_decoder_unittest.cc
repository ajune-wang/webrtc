/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/black_frame_decoder.h"

#include <memory>

#include "api/test/mock_video_decoder.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;

namespace webrtc {

class BlackFrameDecoderTest : public ::testing::Test {
 public:
  BlackFrameDecoderTest()
      : decoder_(
            std::make_unique<BlackFrameDecoder>(SdpVideoFormat("TestFormat"))) {
  }
  ~BlackFrameDecoderTest() override = default;

 protected:
  EncodedImage MakeImage(VideoFrameType type) {
    EncodedImage image;
    image._frameType = type;
    return image;
  }
  std::unique_ptr<BlackFrameDecoder> decoder_;
};

TEST_F(BlackFrameDecoderTest, VP8TooShortReturnsError) {
  BlackFrameDecoder decoder(SdpVideoFormat("VP8"));

  MockDecodedImageCallback decoder_callback;
  ASSERT_EQ(decoder.RegisterDecodeCompleteCallback(&decoder_callback), 0);

  EXPECT_EQ(
      -1, decoder.Decode(MakeImage(VideoFrameType::kVideoFrameKey), false, 0));
}

TEST_F(BlackFrameDecoderTest, VP8) {
  BlackFrameDecoder decoder(SdpVideoFormat("VP8"));

  MockDecodedImageCallback decoder_callback;
  ASSERT_EQ(decoder.RegisterDecodeCompleteCallback(&decoder_callback), 0);

  EncodedImage image = MakeImage(VideoFrameType::kVideoFrameKey);
  // A VP8 payload header for a 640x360 keyframe. Does not include QP.
  uint8_t buffer[] = {
      0x30, 0xb5, 0x00, 0x9d, 0x01, 0x2a, 0x80, 0x02, 0x68, 0x01,
  };

  image.SetEncodedData(EncodedImageBuffer::Create(buffer, sizeof buffer));

  EXPECT_CALL(decoder_callback, Decoded(_, _, _))
      .WillOnce(::testing::Invoke([](VideoFrame& decodedImage,
                                     absl::optional<int32_t> decode_time_ms,
                                     absl::optional<uint8_t> qp) {
        EXPECT_EQ(decodedImage.width(), 640);
        EXPECT_EQ(decodedImage.height(), 360);
        EXPECT_EQ(qp, 0);
      }));
  EXPECT_EQ(0, decoder.Decode(image, false, 0));

  // Second call retains width and height.
  EXPECT_CALL(decoder_callback, Decoded(_, _, _))
      .WillOnce(::testing::Invoke([](VideoFrame& decodedImage,
                                     absl::optional<int32_t> decode_time_ms,
                                     absl::optional<uint8_t> qp) {
        EXPECT_EQ(decodedImage.width(), 640);
        EXPECT_EQ(decodedImage.height(), 360);
        EXPECT_EQ(qp, 0);
      }));
  EXPECT_EQ(
      0, decoder.Decode(MakeImage(VideoFrameType::kVideoFrameDelta), false, 0));
}

TEST_F(BlackFrameDecoderTest, VP9TooShortReturnsError) {
  BlackFrameDecoder decoder(SdpVideoFormat("VP9"));

  MockDecodedImageCallback decoder_callback;
  ASSERT_EQ(decoder.RegisterDecodeCompleteCallback(&decoder_callback), 0);

  EXPECT_EQ(
      -1, decoder.Decode(MakeImage(VideoFrameType::kVideoFrameKey), false, 0));
}

TEST_F(BlackFrameDecoderTest, VP9) {
  BlackFrameDecoder decoder(SdpVideoFormat("VP9"));

  MockDecodedImageCallback decoder_callback;
  ASSERT_EQ(decoder.RegisterDecodeCompleteCallback(&decoder_callback), 0);

  EncodedImage image = MakeImage(VideoFrameType::kVideoFrameKey);
  // Borrowed from Vp9UncompressedHeaderParserTest.
  uint8_t buffer[] = {
      0x87, 0x01, 0x00, 0x00, 0x02, 0x7e, 0x01, 0xdf, 0x02, 0x7f, 0x01, 0xdf,
      0xc6, 0x87, 0x04, 0x83, 0x83, 0x2e, 0x46, 0x60, 0x20, 0x38, 0x0c, 0x06,
      0x03, 0xcd, 0x80, 0xc0, 0x60, 0x9f, 0xc5, 0x46, 0x00, 0x00, 0x00, 0x00,
      0x2e, 0x73, 0xb7, 0xee, 0x22, 0x06, 0x81, 0x82, 0xd4, 0xef, 0xc3, 0x58,
      0x1f, 0x12, 0xd2, 0x7b, 0x28, 0x1f, 0x80, 0xfc, 0x07, 0xe0, 0x00, 0x00};

  image.SetEncodedData(EncodedImageBuffer::Create(buffer, sizeof buffer));

  EXPECT_CALL(decoder_callback, Decoded(_, _, _))
      .WillOnce(::testing::Invoke([](VideoFrame& decodedImage,
                                     absl::optional<int32_t> decode_time_ms,
                                     absl::optional<uint8_t> qp) {
        EXPECT_EQ(decodedImage.width(), 320);
        EXPECT_EQ(decodedImage.height(), 240);
        EXPECT_EQ(qp, 185);
      }));
  EXPECT_EQ(0, decoder.Decode(image, false, 0));

  // Second call retains width and height.
  EXPECT_CALL(decoder_callback, Decoded(_, _, _))
      .WillOnce(::testing::Invoke([](VideoFrame& decodedImage,
                                     absl::optional<int32_t> decode_time_ms,
                                     absl::optional<uint8_t> qp) {
        EXPECT_EQ(decodedImage.width(), 320);
        EXPECT_EQ(decodedImage.height(), 240);
        EXPECT_EQ(qp, 185);
      }));
  EXPECT_EQ(
      0, decoder.Decode(MakeImage(VideoFrameType::kVideoFrameDelta), false, 0));
}

TEST_F(BlackFrameDecoderTest, H264TooShortReturnsError) {
  BlackFrameDecoder decoder(SdpVideoFormat("H264"));

  MockDecodedImageCallback decoder_callback;
  ASSERT_EQ(decoder.RegisterDecodeCompleteCallback(&decoder_callback), 0);

  EXPECT_EQ(
      -1, decoder.Decode(MakeImage(VideoFrameType::kVideoFrameKey), false, 0));
}

TEST_F(BlackFrameDecoderTest, H264) {
  BlackFrameDecoder decoder(SdpVideoFormat("H264"));

  MockDecodedImageCallback decoder_callback;
  ASSERT_EQ(decoder.RegisterDecodeCompleteCallback(&decoder_callback), 0);

  EncodedImage image = MakeImage(VideoFrameType::kVideoFrameKey);
  // Borrowed from PpsParserTest, contains enough of the image slice to contain
  // slice QP.
  const uint8_t buffer[] = {
      0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x80, 0x20, 0xda, 0x01, 0x40, 0x16,
      0xe8, 0x06, 0xd0, 0xa1, 0x35, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x06,
      0xe2, 0x00, 0x00, 0x00, 0x01, 0x65, 0xb8, 0x40, 0xf0, 0x8c, 0x03, 0xf2,
      0x75, 0x67, 0xad, 0x41, 0x64, 0x24, 0x0e, 0xa0, 0xb2, 0x12, 0x1e, 0xf8,
  };

  image.SetEncodedData(EncodedImageBuffer::Create(buffer, sizeof buffer));

  EXPECT_CALL(decoder_callback, Decoded(_, _, _))
      .WillOnce(::testing::Invoke([](VideoFrame& decodedImage,
                                     absl::optional<int32_t> decode_time_ms,
                                     absl::optional<uint8_t> qp) {
        EXPECT_EQ(decodedImage.width(), 1280);
        EXPECT_EQ(decodedImage.height(), 720);
        EXPECT_EQ(qp, 35);
      }));
  EXPECT_EQ(0, decoder.Decode(image, false, 0));

  // Second call retains width and height.
  EXPECT_CALL(decoder_callback, Decoded(_, _, _))
      .WillOnce(::testing::Invoke([](VideoFrame& decodedImage,
                                     absl::optional<int32_t> decode_time_ms,
                                     absl::optional<uint8_t> qp) {
        EXPECT_EQ(decodedImage.width(), 1280);
        EXPECT_EQ(decodedImage.height(), 720);
        EXPECT_EQ(qp, 35);
      }));
  EXPECT_EQ(
      0, decoder.Decode(MakeImage(VideoFrameType::kVideoFrameDelta), false, 0));
}

TEST_F(BlackFrameDecoderTest, AV1UnsupportedReturns2x2) {
  BlackFrameDecoder decoder(SdpVideoFormat("AV1"));

  MockDecodedImageCallback decoder_callback;
  ASSERT_EQ(decoder.RegisterDecodeCompleteCallback(&decoder_callback), 0);

  EXPECT_CALL(decoder_callback, Decoded(_, _, _))
      .WillOnce(::testing::Invoke([](VideoFrame& decodedImage,
                                     absl::optional<int32_t> decode_time_ms,
                                     absl::optional<uint8_t> qp) {
        EXPECT_EQ(decodedImage.width(), 2);
        EXPECT_EQ(decodedImage.height(), 2);
        EXPECT_EQ(qp, 0);
      }));
  EXPECT_EQ(
      0, decoder.Decode(MakeImage(VideoFrameType::kVideoFrameKey), false, 0));

  EXPECT_CALL(decoder_callback, Decoded(_, _, _))
      .WillOnce(::testing::Invoke([](VideoFrame& decodedImage,
                                     absl::optional<int32_t> decode_time_ms,
                                     absl::optional<uint8_t> qp) {
        EXPECT_EQ(decodedImage.width(), 2);
        EXPECT_EQ(decodedImage.height(), 2);
        EXPECT_EQ(qp, 0);
      }));
  EXPECT_EQ(
      0, decoder.Decode(MakeImage(VideoFrameType::kVideoFrameDelta), false, 0));

  // Second call retains width and height.
  EXPECT_CALL(decoder_callback, Decoded(_, _, _))
      .WillOnce(::testing::Invoke([](VideoFrame& decodedImage,
                                     absl::optional<int32_t> decode_time_ms,
                                     absl::optional<uint8_t> qp) {
        EXPECT_EQ(decodedImage.width(), 2);
        EXPECT_EQ(decodedImage.height(), 2);
        EXPECT_EQ(qp, 0);
      }));
  EXPECT_EQ(
      0, decoder.Decode(MakeImage(VideoFrameType::kVideoFrameDelta), false, 0));
}

}  // namespace webrtc
