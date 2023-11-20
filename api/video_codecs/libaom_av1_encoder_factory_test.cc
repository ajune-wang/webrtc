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

#include <cstdio>
#include <utility>
#include <vector>

#include "api/video/i420_buffer.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/codecs/av1/dav1d_decoder.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/frame_writer.h"

namespace webrtc {
namespace {
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::NotNull;
using Cbr = VideoEncoderInterface::FrameEncodeSettings::Cbr;
using Cqp = VideoEncoderInterface::FrameEncodeSettings::Cqp;
using DroppedFrame = VideoEncoderInterface::DroppedFrame;
using EncodedData = VideoEncoderInterface::EncodedData;
using EncodeResult = VideoEncoderInterface::EncodeResult;
using EncodeResultCallback = VideoEncoderInterface::EncodeResultCallback;
using FrameType = VideoEncoderInterface::FrameType;
using RcMode = VideoEncoderInterface::RateControlMode;

std::unique_ptr<test::FrameReader> CreateFrameReader() {
  return CreateY4mFrameReader(
      test::ResourcePath("reference_video_640x360_30fps", "y4m"),
      test::YuvFrameReaderImpl::RepeatMode::kPingPong);
}

std::string OutPath() {
  std::string res = test::OutputPath();
  res += "frame_dump/";
  RTC_CHECK(test::DirExists(res) || test::CreateDir(res));
  return res;
}

void WriteFrame(std::string name, const VideoFrame& frame) {
  std::string out;
  out.reserve(128);
  out += name;
  out += "_";
  out += std::to_string(frame.width());
  out += "x";
  out += std::to_string(frame.height());
  out += ".jpg";
  test::JpegFrameWriter writer(out);
  writer.WriteFrame(frame, /*quality=*/100);
}

class EncodeResults {
 public:
  EncodeResultCallback CallBack() {
    return [&](const EncodeResult& result) { results_.push_back(result); };
  }

  EncodedData* FrameAt(int index) {
    if (index < 0 || index > static_cast<int>(results_.size())) {
      return nullptr;
    }
    return std::get_if<EncodedData>(&results_[index]);
  }

  DroppedFrame* DropAt(int index) {
    if (index < 0 || index > static_cast<int>(results_.size())) {
      return nullptr;
    }
    return std::get_if<DroppedFrame>(&results_[index]);
  }

 private:
  std::vector<EncodeResult> results_;
};

class Av1Decoder : public DecodedImageCallback {
 public:
  Av1Decoder() : Av1Decoder("") {}

  Av1Decoder(const std::string& name)
      : decoder_(CreateDav1dDecoder()), file_name_(name) {
    decoder_->Configure({});
    decoder_->RegisterDecodeCompleteCallback(this);

    if (!file_name_.empty()) {
      std::string out = OutPath();
      RTC_CHECK(test::DirExists(out) || test::CreateDir(out));
      out += file_name_;
      out += "_raw.av1";
      RTC_CHECK(raw_out_file_ = fopen(out.c_str(), "wb"));
    }
  }

  ~Av1Decoder() {
    fclose(raw_out_file_);
  }

  // DecodedImageCallback
  int32_t Decoded(VideoFrame& frame) override {
    decode_result_ = std::make_unique<VideoFrame>(std::move(frame));
    return 0;
  }

  VideoFrame Decode(const EncodedData& encoded_data) {
    EncodedImage img;
    img.SetEncodedData(encoded_data.bitstream_data);
    decoder_->Decode(img, /*dont_care=*/0);
    VideoFrame res(std::move(*decode_result_));
    Write(encoded_data, res);
    return res;
  }

 private:
  void Write(const EncodedData& ed, const VideoFrame& vf) {
    if (file_name_.empty()) {
      return;
    }
    std::string name;
    name.reserve(128);
    name += file_name_;
    name += "_";
    name += std::to_string(frames_written_++);
    name += "_S";
    name += std::to_string(ed.spatial_id);
    WriteFrame(name, vf);
    fwrite(ed.bitstream_data->data(), 1, ed.bitstream_data->size(), raw_out_file_);  
  }
  std::unique_ptr<VideoDecoder> decoder_;
  std::unique_ptr<VideoFrame> decode_result_;
  std::string file_name_;
  int frames_written_ = 0;
  FILE* raw_out_file_;
};

double Psnr(const rtc::scoped_refptr<I420BufferInterface>& ref_buffer,
            const VideoFrame& decoded_frame) {
  return I420PSNR(*ref_buffer, *decoded_frame.video_frame_buffer()->ToI420());
}

static constexpr VideoEncoderFactoryInterface::StaticEncoderSettings
    kCbrEncoderSettings{
        .max_encode_dimensions = {.width = 1920, .height = 1080},
        .encoding_format = {.sub_sampling = EncodingFormat::SubSampling::k420,
                            .bit_depth = 8},
        .rc_mode = RcMode::kCbr,
        .max_number_of_threads = 1,
    };

TEST(LibaomAv1EncoderFactory, CodecName) {
  EXPECT_THAT(LibaomAv1EncoderFactory().CodecName(), Eq("AV1"));
}

TEST(LibaomAv1EncoderFactory, CodecSpecifics) {
  EXPECT_THAT(LibaomAv1EncoderFactory().CodecSpecifics(), IsEmpty());
}

TEST(LibaomAv1Encoder, EncodeKeyframe) {
  auto frame_reader = CreateFrameReader();
  auto encoder =
      LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;
  Av1Decoder decoder;

  auto raw_frame = frame_reader->PullFrame();
  EXPECT_TRUE(encoder->Encode(
      raw_frame, {},
      {{.rate_options = Cbr{.duration = TimeDelta::Millis(100),
                            .target_bitrate = DataRate::KilobitsPerSec(1000)},
        .frame_type = FrameType::kKeyframe,
        .resolution = {640, 360}}},
      res.CallBack()));

  ASSERT_THAT(res.FrameAt(0), NotNull());
  VideoFrame decoded_frame = decoder.Decode(*res.FrameAt(0));
  EXPECT_THAT(decoded_frame.width(), Eq(640));
  EXPECT_THAT(decoded_frame.height(), Eq(360));
  EXPECT_THAT(Psnr(raw_frame, decoded_frame), Gt(40));
}

TEST(LibaomAv1Encoder, EncodeKeyframeWithScaling) {
  auto frame_reader = CreateFrameReader();
  auto encoder =
      LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;
  Av1Decoder decoder;

  auto raw_frame = frame_reader->PullFrame();
  EXPECT_TRUE(encoder->Encode(
      raw_frame, {},
      {{.rate_options = Cbr{.duration = TimeDelta::Millis(100),
                            .target_bitrate = DataRate::KilobitsPerSec(1000)},
        .frame_type = FrameType::kKeyframe,
        .resolution = {320, 180}}},
      res.CallBack()));

  ASSERT_THAT(res.FrameAt(0), NotNull());
  VideoFrame decoded_frame = decoder.Decode(*res.FrameAt(0));
  EXPECT_THAT(decoded_frame.width(), Eq(320));
  EXPECT_THAT(decoded_frame.height(), Eq(180));
  EXPECT_THAT(Psnr(raw_frame, decoded_frame), Gt(40));
}

TEST(LibaomAv1Encoder, EncodeSpatialLayerKeyframe) {
  auto frame_reader = CreateFrameReader();
  auto encoder =
      LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
  EXPECT_TRUE(encoder->Encode(
      frame, {},
      {{.rate_options = Cbr{.duration = TimeDelta::Millis(100),
                            .target_bitrate = DataRate::KilobitsPerSec(500)},
        .frame_type = FrameType::kKeyframe,
        .spatial_id = 0,
        .resolution = {320, 180}},
       {.rate_options = Cbr{.duration = TimeDelta::Millis(100),
                            .target_bitrate = DataRate::KilobitsPerSec(500)},
        .spatial_id = 1,
        .resolution = {640, 360},
        .reference_buffers = {0}}},
      res.CallBack()));

  Av1Decoder decoder;

  EXPECT_THAT(res.FrameAt(0), Field(&EncodedData::spatial_id, 0));
  EXPECT_THAT(res.FrameAt(1), Field(&EncodedData::spatial_id, 1));
  EXPECT_THAT(res.FrameAt(1),
              Field(&EncodedData::referenced_buffers, ElementsAre(0)));
  decoder.Decode(*res.FrameAt(0));
  EXPECT_THAT(Psnr(frame, decoder.Decode(*res.FrameAt(1))), Gt(40));
}

TEST(LibaomAv1Encoder, ResolutionUpswitch) {
  auto frame_reader = CreateFrameReader();
  auto encoder =
      LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  EXPECT_TRUE(encoder->Encode(
      frame_reader->PullFrame(), {},
      {
          {.rate_options = Cbr{.duration = TimeDelta::Millis(100),
                               .target_bitrate = DataRate::KilobitsPerSec(500)},
           .frame_type = FrameType::kKeyframe,
           .spatial_id = 0,
           .resolution = {320, 180}},
      },
      res.CallBack()));

  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
  EXPECT_TRUE(encoder->Encode(
      frame, {},
      {{.rate_options = Cbr{.duration = TimeDelta::Millis(100),
                            .target_bitrate = DataRate::KilobitsPerSec(500)},
        .spatial_id = 0,
        .resolution = {640, 360},
        .reference_buffers = {0}}},
      res.CallBack()));

  EXPECT_THAT(res.FrameAt(0), Field(&EncodedData::spatial_id, 0));
  EXPECT_THAT(res.FrameAt(1), Field(&EncodedData::spatial_id, 0));

  Av1Decoder decoder;
  decoder.Decode(*res.FrameAt(0));
  EXPECT_THAT(Psnr(frame, decoder.Decode(*res.FrameAt(1))), Gt(40));
}

TEST(LibaomAv1Encoder, TempoSpatial) {
  auto frame_reader = CreateFrameReader();
  auto encoder =
      LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  const Cbr kCbr10Fps = {.duration = TimeDelta::Millis(100),
                         .target_bitrate = DataRate::KilobitsPerSec(500)};

  const Cbr kCbr20Fps = {.duration = TimeDelta::Millis(50),
                         .target_bitrate = DataRate::KilobitsPerSec(2000)};

  ASSERT_TRUE(encoder->Encode(frame_reader->PullFrame(), {},
                              {{.rate_options = kCbr10Fps,
                                .frame_type = FrameType::kKeyframe,
                                .spatial_id = 0,
                                .resolution = {160, 90}},
                               {.rate_options = kCbr10Fps,
                                .spatial_id = 1,
                                .resolution = {320, 180},
                                .reference_buffers = {0},
                                .update_buffers = {1}},
                               {.rate_options = kCbr20Fps,
                                .spatial_id = 2,
                                .resolution = {640, 360},
                                .reference_buffers = {1},
                                .update_buffers = {2}}},
                              res.CallBack()));

  ASSERT_TRUE(encoder->Encode(frame_reader->PullFrame(), {},
                              {{.rate_options = kCbr20Fps,
                                .spatial_id = 2,
                                .resolution = {640, 360},
                                .reference_buffers = {2},
                                .update_buffers = {2}}},
                              res.CallBack()));

  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
  ASSERT_TRUE(encoder->Encode(frame, {},
                              {{.rate_options = kCbr10Fps,
                                .spatial_id = 0,
                                .resolution = {160, 90},
                                .reference_buffers = {0},
                                .update_buffers = {0}},
                               {.rate_options = kCbr10Fps,
                                .spatial_id = 1,
                                .resolution = {320, 180},
                                .reference_buffers = {0},
                                .update_buffers = {1}},
                               {.rate_options = kCbr20Fps,
                                .spatial_id = 2,
                                .resolution = {640, 360},
                                .reference_buffers = {1, 2},
                                .update_buffers = {2}}},
                              res.CallBack()));

  Av1Decoder decoder;
  VideoFrame f0 = decoder.Decode(*res.FrameAt(0));
  EXPECT_THAT(f0.width(), Eq(160));
  EXPECT_THAT(f0.height(), Eq(90));

  VideoFrame f1 = decoder.Decode(*res.FrameAt(1));
  EXPECT_THAT(f1.width(), Eq(320));
  EXPECT_THAT(f1.height(), Eq(180));

  VideoFrame f2 = decoder.Decode(*res.FrameAt(2));
  EXPECT_THAT(f2.width(), Eq(640));
  EXPECT_THAT(f2.height(), Eq(360));

  VideoFrame f3 = decoder.Decode(*res.FrameAt(3));
  EXPECT_THAT(f3.width(), Eq(640));
  EXPECT_THAT(f3.height(), Eq(360));

  VideoFrame f4 = decoder.Decode(*res.FrameAt(4));
  EXPECT_THAT(f4.width(), Eq(160));
  EXPECT_THAT(f4.height(), Eq(90));

  VideoFrame f5 = decoder.Decode(*res.FrameAt(5));
  EXPECT_THAT(f5.width(), Eq(320));
  EXPECT_THAT(f5.height(), Eq(180));

  VideoFrame f6 = decoder.Decode(*res.FrameAt(6));
  EXPECT_THAT(f6.width(), Eq(640));
  EXPECT_THAT(f6.height(), Eq(360));

  EXPECT_THAT(Psnr(frame, f6), Gt(40));
}

TEST(LibaomAv1Encoder, InvertedTempoSpatial) {
  auto frame_reader = CreateFrameReader();
  auto encoder =
      LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  ASSERT_TRUE(encoder->Encode(
      frame_reader->PullFrame(), {},
      {{.rate_options = Cbr{.duration = TimeDelta::Millis(100),
                            .target_bitrate = DataRate::KilobitsPerSec(150)},
        .frame_type = FrameType::kKeyframe,
        .spatial_id = 0,
        .resolution = {320, 180}},
       {.rate_options = Cbr{.duration = TimeDelta::Millis(200),
                            .target_bitrate = DataRate::KilobitsPerSec(500)},
        .spatial_id = 1,
        .resolution = {640, 360},
        .reference_buffers = {0},
        .update_buffers = {1}}},
      res.CallBack()));

  // TODO: Wait for https://aomedia-review.googlesource.com/c/aom/+/183901
  ASSERT_TRUE(encoder->Encode(
      frame_reader->PullFrame(), {},
      {{.rate_options = Cbr{.duration = TimeDelta::Millis(100),
                            .target_bitrate = DataRate::KilobitsPerSec(150)},
        .spatial_id = 0,
        .resolution = {320, 180},
        .reference_buffers = {0},
        .update_buffers = {0}}},
      res.CallBack()));

  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
  ASSERT_TRUE(encoder->Encode(
      frame, {},
      {{.rate_options = Cbr{.duration = TimeDelta::Millis(100),
                            .target_bitrate = DataRate::KilobitsPerSec(150)},
        .spatial_id = 0,
        .resolution = {320, 180},
        .reference_buffers = {0},
        .update_buffers = {0}},
       {.rate_options = Cbr{.duration = TimeDelta::Millis(200),
                            .target_bitrate = DataRate::KilobitsPerSec(5000)},
        .spatial_id = 1,
        .resolution = {640, 360},
        .reference_buffers = {1, 0},
        .update_buffers = {1}}},
      res.CallBack()));

  Av1Decoder decoder;

  VideoFrame f0 = decoder.Decode(*res.FrameAt(0));
  EXPECT_THAT(f0.width(), Eq(320));
  EXPECT_THAT(f0.height(), Eq(180));

  VideoFrame f1 = decoder.Decode(*res.FrameAt(1));
  EXPECT_THAT(f1.width(), Eq(640));
  EXPECT_THAT(f1.height(), Eq(360));

  VideoFrame f2 = decoder.Decode(*res.FrameAt(2));
  EXPECT_THAT(f2.width(), Eq(320));
  EXPECT_THAT(f2.height(), Eq(180));

  VideoFrame f3 = decoder.Decode(*res.FrameAt(3));
  EXPECT_THAT(f3.width(), Eq(320));
  EXPECT_THAT(f3.height(), Eq(180));

  VideoFrame f4 = decoder.Decode(*res.FrameAt(4));
  EXPECT_THAT(f4.width(), Eq(640));
  EXPECT_THAT(f4.height(), Eq(360));
}

TEST(LibaomAv1Encoder, L3T1_KEY) {
  auto frame_reader = CreateFrameReader();
  auto encoder =
      LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  Av1Decoder decoder("L3T1_KEY");
  const Cbr kCbr = {.duration = TimeDelta::Millis(100),
                    .target_bitrate = DataRate::KilobitsPerSec(500)};

  ASSERT_TRUE(encoder->Encode(frame_reader->PullFrame(), {},
                              {{.rate_options = kCbr,
                                .frame_type = FrameType::kKeyframe,
                                .spatial_id = 0,
                                .resolution = {160, 90}},
                               {.rate_options = kCbr,
                                .spatial_id = 1,
                                .resolution = {320, 180},
                                .reference_buffers = {0},
                                .update_buffers = {1}},
                               {.rate_options = kCbr,
                                .spatial_id = 2,
                                .resolution = {640, 360},
                                .reference_buffers = {1},
                                .update_buffers = {2}}},
                              res.CallBack()));
  VideoFrame f0 = decoder.Decode(*res.FrameAt(0));
  EXPECT_THAT(f0.width(), Eq(160));
  EXPECT_THAT(f0.height(), Eq(90));

  VideoFrame f1 = decoder.Decode(*res.FrameAt(1));
  EXPECT_THAT(f1.width(), Eq(320));
  EXPECT_THAT(f1.height(), Eq(180));

  VideoFrame f2 = decoder.Decode(*res.FrameAt(2));
  EXPECT_THAT(f2.width(), Eq(640));
  EXPECT_THAT(f2.height(), Eq(360));

  for (int i = 0; i < 12; i += 3) {
    rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
    ASSERT_TRUE(encoder->Encode(frame, {},
                                {{.rate_options = kCbr,
                                  .spatial_id = 0,
                                  .resolution = {160, 90},
                                  .reference_buffers = {0},
                                  .update_buffers = {0}},
                                 {.rate_options = kCbr,
                                  .spatial_id = 1,
                                  .resolution = {320, 180},
                                  .reference_buffers = {1},
                                  .update_buffers = {1}},
                                 {.rate_options = kCbr,
                                  .spatial_id = 2,
                                  .resolution = {640, 360},
                                  .reference_buffers = {2},
                                  .update_buffers = {2}}},
                                res.CallBack()));

    // VideoFrame f3 = decoder.Decode(*res.FrameAt(3+i));
    // EXPECT_THAT(f3.width(), Eq(160));
    // EXPECT_THAT(f3.height(), Eq(90));

    // VideoFrame f4 = decoder.Decode(*res.FrameAt(4+i));
    // EXPECT_THAT(f4.width(), Eq(320));
    // EXPECT_THAT(f4.height(), Eq(180));

    VideoFrame f5 = decoder.Decode(*res.FrameAt(5 + i));
    EXPECT_THAT(f5.width(), Eq(640));
    EXPECT_THAT(f5.height(), Eq(360));
  }

  // EXPECT_THAT(Psnr(frame, f5), Gt(40));
}

}  // namespace
}  // namespace webrtc
