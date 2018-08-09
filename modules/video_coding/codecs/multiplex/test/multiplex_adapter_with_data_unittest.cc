/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "absl/memory/memory.h"
#include "api/test/mock_video_decoder_factory.h"
#include "api/test/mock_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "common_video/include/video_frame_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/base/mediaconstants.h"
#include "modules/video_coding/codecs/multiplex/include/multiplex_decoder_adapter.h"
#include "modules/video_coding/codecs/multiplex/include/multiplex_encoded_image_packer.h"
#include "modules/video_coding/codecs/multiplex/include/multiplex_encoder_adapter.h"
#include "modules/video_coding/codecs/multiplex/include/multiplex_video_frame_buffer.h"
#include "modules/video_coding/codecs/test/video_codec_unittest.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "rtc_base/keep_ref_until_done.h"
#include "test/video_codec_settings.h"

using testing::_;
using testing::Return;

namespace webrtc {

constexpr const char* kMultiplexAssociatedCodecName = cricket::kVp9CodecName;
const VideoCodecType kMultiplexAssociatedCodecType =
    PayloadStringToCodecType(kMultiplexAssociatedCodecName);

class TestMultiplexWithDataAdapter : public VideoCodecUnitTest {
 public:
  TestMultiplexWithDataAdapter()
      : decoder_factory_(new webrtc::MockVideoDecoderFactory),
        encoder_factory_(new webrtc::MockVideoEncoderFactory) {}

 protected:
  std::unique_ptr<VideoDecoder> CreateDecoder() override {
    return absl::make_unique<MultiplexDecoderAdapter>(
        decoder_factory_.get(), SdpVideoFormat(kMultiplexAssociatedCodecName), true);
  }

  std::unique_ptr<VideoEncoder> CreateEncoder() override {
    return absl::make_unique<MultiplexEncoderAdapter>(
        encoder_factory_.get(), SdpVideoFormat(kMultiplexAssociatedCodecName), true);
  }

  void ModifyCodecSettings(VideoCodec* codec_settings) override {
    webrtc::test::CodecSettings(kMultiplexAssociatedCodecType, codec_settings);
    codec_settings->VP9()->numberOfTemporalLayers = 1;
    codec_settings->VP9()->numberOfSpatialLayers = 1;
    codec_settings->codecType = webrtc::kVideoCodecMultiplex;
  }
  
  rtc::scoped_refptr<I420BufferInterface> CreateI420FrameBuffer() {
    VideoFrame* input_frame = NextInputFrame();
    rtc::scoped_refptr<webrtc::I420BufferInterface> yuv_buffer =
      input_frame->video_frame_buffer()->ToI420();
    return yuv_buffer;
  }

  rtc::scoped_refptr<I420ABufferInterface> CreateI420AFrameBuffer() {
    rtc::scoped_refptr<webrtc::I420BufferInterface> yuv_buffer = CreateI420FrameBuffer();
    rtc::scoped_refptr<I420ABufferInterface> yuva_buffer = WrapI420ABuffer(
        yuv_buffer->width(), yuv_buffer->height(), yuv_buffer->DataY(),
        yuv_buffer->StrideY(), yuv_buffer->DataU(), yuv_buffer->StrideU(),
        yuv_buffer->DataV(), yuv_buffer->StrideV(), yuv_buffer->DataY(),
        yuv_buffer->StrideY(), rtc::KeepRefUntilDone(yuv_buffer));
    return yuva_buffer;
  }
  
  std::unique_ptr<VideoFrame> CreateDataAugmentedMultiplexInputFrame(rtc::scoped_refptr<VideoFrameBuffer> yuv_buffer) {
    rtc::scoped_refptr<I420ABufferInterface> yuva_buffer = CreateI420AFrameBuffer();
    uint8_t* data = new uint8_t[16];
    for (int i = 0; i < 16; i++) {
      data[i] = i;
    }
    rtc::scoped_refptr<MultiplexVideoFrameBuffer> multiplex_video_buffer =
        new rtc::RefCountedObject<MultiplexVideoFrameBuffer>(
          yuv_buffer,
          data,
          16);
    return absl::WrapUnique<VideoFrame>(
        new VideoFrame(multiplex_video_buffer, 123 /* RTP timestamp */,
                       345 /* render_time_ms */, kVideoRotation_0));
  }

  std::unique_ptr<VideoFrame> ExtractAXXFrame(rtc::scoped_refptr<VideoFrameBuffer> video_buffer) {
    MultiplexVideoFrameBuffer* multiplexBuffer = static_cast<MultiplexVideoFrameBuffer *>(video_buffer.get());
    auto underlying_buffer = multiplexBuffer->GetVideoFrameBuffer();
    const I420ABufferInterface* yuva_buffer = underlying_buffer->GetI420A();
    rtc::scoped_refptr<I420BufferInterface> axx_buffer = WrapI420Buffer(
        yuva_buffer->width(), yuva_buffer->height(), yuva_buffer->DataA(),
        yuva_buffer->StrideA(), yuva_buffer->DataU(), yuva_buffer->StrideU(),
        yuva_buffer->DataV(), yuva_buffer->StrideV(),
        rtc::KeepRefUntilDone(yuva_buffer));
    return absl::WrapUnique<VideoFrame>(
        new VideoFrame(axx_buffer, 123 /* RTP timestamp */,
                       345 /* render_time_ms */, kVideoRotation_0));
  }

 private:
  void SetUp() override {
    EXPECT_CALL(*decoder_factory_, Die());
    // The decoders/encoders will be owned by the caller of
    // CreateVideoDecoder()/CreateVideoEncoder().
    VideoDecoder* decoder1 = VP9Decoder::Create().release();
    VideoDecoder* decoder2 = VP9Decoder::Create().release();
    EXPECT_CALL(*decoder_factory_, CreateVideoDecoderProxy(_))
        .WillOnce(Return(decoder1))
        .WillOnce(Return(decoder2));

    EXPECT_CALL(*encoder_factory_, Die());
    VideoEncoder* encoder1 = VP9Encoder::Create().release();
    VideoEncoder* encoder2 = VP9Encoder::Create().release();
    EXPECT_CALL(*encoder_factory_, CreateVideoEncoderProxy(_))
        .WillOnce(Return(encoder1))
        .WillOnce(Return(encoder2));

    VideoCodecUnitTest::SetUp();
  }

  const std::unique_ptr<webrtc::MockVideoDecoderFactory> decoder_factory_;
  const std::unique_ptr<webrtc::MockVideoEncoderFactory> encoder_factory_;
};

TEST_F(TestMultiplexWithDataAdapter, ConstructAndDestructDecoder) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Release());
}

TEST_F(TestMultiplexWithDataAdapter, ConstructAndDestructEncoder) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Release());
}
  
TEST_F(TestMultiplexWithDataAdapter, EncodeDecodeDataAugmentedAlphaFrame) {
  rtc::scoped_refptr<I420ABufferInterface> yuva_buffer = CreateI420AFrameBuffer();
  std::unique_ptr<VideoFrame> data_augmented_frame = CreateDataAugmentedMultiplexInputFrame(yuva_buffer);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*data_augmented_frame, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_EQ(kVideoCodecMultiplex, codec_specific_info.codecType);
    
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr, 0));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  EXPECT_GT(I420PSNR(data_augmented_frame.get(), decoded_frame.get()), 36);
    
  // Find PSNR for AXX bits.
  std::unique_ptr<VideoFrame> input_axx_frame =
      ExtractAXXFrame(data_augmented_frame->video_frame_buffer());
  std::unique_ptr<VideoFrame> output_axx_frame =
      ExtractAXXFrame(decoded_frame->video_frame_buffer());
  EXPECT_GT(I420PSNR(input_axx_frame.get(), output_axx_frame.get()), 47);
  
  // Check the data portion
  MultiplexVideoFrameBuffer* multiplex_buffer = static_cast<MultiplexVideoFrameBuffer*>(
    decoded_frame->video_frame_buffer().get());
  EXPECT_EQ(multiplex_buffer->GetAugmentingDataSize(), 16);
  uint8_t* data = multiplex_buffer->GetAndReleaseAugmentingData();
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(data[i], i);
  }
  delete []data;
}
  
TEST_F(TestMultiplexWithDataAdapter, EncodeDecodeDataAugmentedFrame) {
  rtc::scoped_refptr<I420BufferInterface> yuv_buffer = CreateI420FrameBuffer();
  std::unique_ptr<VideoFrame> data_augmented_frame = CreateDataAugmentedMultiplexInputFrame(yuv_buffer);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*data_augmented_frame, nullptr, nullptr));
  EncodedImage encoded_frame;
  CodecSpecificInfo codec_specific_info;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));
  EXPECT_EQ(kVideoCodecMultiplex, codec_specific_info.codecType);
    
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr, 0));
  std::unique_ptr<VideoFrame> decoded_frame;
  absl::optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  EXPECT_GT(I420PSNR(data_augmented_frame.get(), decoded_frame.get()), 36);
    
  // Check the data portion
  MultiplexVideoFrameBuffer* multiplex_buffer = static_cast<MultiplexVideoFrameBuffer*>(
                                                                                          decoded_frame->video_frame_buffer().get());
  EXPECT_EQ(multiplex_buffer->GetAugmentingDataSize(), 16);
  uint8_t* data = multiplex_buffer->GetAndReleaseAugmentingData();
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(data[i], i);
  }
  delete []data;
}
}  // namespace webrtc
