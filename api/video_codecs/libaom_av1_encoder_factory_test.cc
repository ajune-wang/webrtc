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
using ::testing::Lt;
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

class EncodeResults {
 public:
  EncodeResultCallback CallBack() {
    return [&](const EncodeResult& result) { results_.push_back(result); };
  }

  EncodedData* FrameAt(int index) {
    if (index < 0 || index > static_cast<int>(results_.size())) {
      RTC_CHECK(false);
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
      out += file_name_;
      out += "_raw.av1";
      RTC_CHECK(raw_out_file_ = fopen(out.c_str(), "wb"));
    }
  }

  ~Av1Decoder() {
    if (raw_out_file_) {
      fclose(raw_out_file_);
    }
  }

  // DecodedImageCallback
  int32_t Decoded(VideoFrame& frame) override {
    decode_result_ = std::make_unique<VideoFrame>(std::move(frame));
    return 0;
  }

  VideoFrame Decode(const EncodedData& encoded_data) {
    EncodedImage img;
    img.SetEncodedData(encoded_data.bitstream_data);
    if (raw_out_file_) {
      fwrite(encoded_data.bitstream_data->data(), 1,
             encoded_data.bitstream_data->size(), raw_out_file_);
    }
    // RTC_CHECK(decoder_->Decode(img, /*dont_care=*/0) == 0);
    decoder_->Decode(img, /*dont_care=*/0);
    VideoFrame res(std::move(*decode_result_));
    return res;
  }

 private:
  std::unique_ptr<VideoDecoder> decoder_;
  std::unique_ptr<VideoFrame> decode_result_;
  std::string file_name_;
  FILE* raw_out_file_ = nullptr;
};

class FrameEncoderSettingsBuilder {
 public:
  FrameEncoderSettingsBuilder& Key() {
    frame_encode_settings_.frame_type = FrameType::kKeyframe;
    return *this;
  }

  FrameEncoderSettingsBuilder& Start() {
    frame_encode_settings_.frame_type = FrameType::kStartFrame;
    return *this;
  }

  FrameEncoderSettingsBuilder& Delta() {
    frame_encode_settings_.frame_type = FrameType::kStartFrame;
    return *this;
  }

  FrameEncoderSettingsBuilder& Rate(
      const absl::variant<Cqp, Cbr>& rate_options) {
    frame_encode_settings_.rate_options = rate_options;
    return *this;
  }

  FrameEncoderSettingsBuilder& T(int id) {
    frame_encode_settings_.temporal_id = id;
    return *this;
  }

  FrameEncoderSettingsBuilder& S(int id) {
    frame_encode_settings_.spatial_id = id;
    return *this;
  }

  FrameEncoderSettingsBuilder& Res(int width, int height) {
    frame_encode_settings_.resolution = {width, height};
    return *this;
  }

  FrameEncoderSettingsBuilder& Ref(const std::vector<int>& ref) {
    frame_encode_settings_.reference_buffers = ref;
    return *this;
  }

  FrameEncoderSettingsBuilder& Upd(const std::vector<int>& upd) {
    frame_encode_settings_.update_buffers = upd;
    return *this;
  }

  VideoEncoderInterface::FrameEncodeSettings Build() {
    return frame_encode_settings_;
  }

 private:
  VideoEncoderInterface::FrameEncodeSettings frame_encode_settings_;
};

using Fb = FrameEncoderSettingsBuilder;

// For reasonable debug printout when an EXPECT fail.
struct Resolution {
  Resolution(const VideoFrame& frame)
      : width(frame.width()), height(frame.height()) {}

  friend void PrintTo(const Resolution& res, std::ostream* os) {
    *os << "(width: " << res.width << " height: " << res.height << ")";
  }

  int width;
  int height;
};

MATCHER_P2(ResolutionIs, width, height, "") {
  return arg.width == width && arg.height == height;
}

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

static constexpr VideoEncoderFactoryInterface::StaticEncoderSettings
    kCqpEncoderSettings{
        .max_encode_dimensions = {.width = 1920, .height = 1080},
        .encoding_format = {.sub_sampling = EncodingFormat::SubSampling::k420,
                            .bit_depth = 8},
        .rc_mode = RcMode::kCqp,
        .max_number_of_threads = 1,
    };

static constexpr Cbr kCbr{.duration = TimeDelta::Millis(100),
                          .target_bitrate = DataRate::KilobitsPerSec(1000)};

TEST(LibaomAv1EncoderFactory, CodecName) {
  EXPECT_THAT(LibaomAv1EncoderFactory().CodecName(), Eq("AV1"));
}

TEST(LibaomAv1EncoderFactory, CodecSpecifics) {
  EXPECT_THAT(LibaomAv1EncoderFactory().CodecSpecifics(), IsEmpty());
}

TEST(LibaomAv1Encoder, EncodeKeyframe) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;
  Av1Decoder dec;

  auto raw_frame = frame_reader->PullFrame();

  EXPECT_TRUE(enc->Encode(raw_frame, {},
                          {Fb().Key().Rate(kCbr).Res(640, 360).Build()},
                          res.CallBack()));

  ASSERT_THAT(res.FrameAt(0), NotNull());
  VideoFrame decoded_frame = dec.Decode(*res.FrameAt(0));
  EXPECT_THAT(Resolution(decoded_frame), ResolutionIs(640, 360));
  EXPECT_THAT(Psnr(raw_frame, decoded_frame), Gt(40));
}

TEST(LibaomAv1Encoder, ResolutionSwitching) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  rtc::scoped_refptr<I420Buffer> in0 = frame_reader->PullFrame();
  EXPECT_TRUE(enc->Encode(
      in0, {}, {Fb().Rate(kCbr).Res(360, 180).Key().Build()}, res.CallBack()));

  rtc::scoped_refptr<I420Buffer> in1 = frame_reader->PullFrame();
  EXPECT_TRUE(enc->Encode(in1, {},
                          {Fb().Rate(kCbr).Res(640, 360).Ref({0}).Build()},
                          res.CallBack()));

  rtc::scoped_refptr<I420Buffer> in2 = frame_reader->PullFrame();
  EXPECT_TRUE(enc->Encode(in2, {},
                          {Fb().Rate(kCbr).Res(160, 90).Ref({0}).Build()},
                          res.CallBack()));

  EXPECT_THAT(res.FrameAt(0), Field(&EncodedData::spatial_id, 0));
  EXPECT_THAT(res.FrameAt(1), Field(&EncodedData::spatial_id, 0));
  EXPECT_THAT(res.FrameAt(2), Field(&EncodedData::spatial_id, 0));

  Av1Decoder dec;
  VideoFrame f0 = dec.Decode(*res.FrameAt(0));
  EXPECT_THAT(Resolution(f0), ResolutionIs(360, 180));
  EXPECT_THAT(Psnr(in0, f0), Gt(40));

  VideoFrame f1 = dec.Decode(*res.FrameAt(1));
  EXPECT_THAT(Resolution(f1), ResolutionIs(640, 360));
  EXPECT_THAT(Psnr(in1, f1), Gt(40));

  VideoFrame f2 = dec.Decode(*res.FrameAt(2));
  EXPECT_THAT(Resolution(f2), ResolutionIs(160, 90));
  EXPECT_THAT(Psnr(in2, f2), Gt(40));
}

TEST(LibaomAv1Encoder, TempoSpatial) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  ASSERT_TRUE(enc->Encode(
      frame_reader->PullFrame(), {},
      {Fb().Rate(kCbr).Res(160, 90).S(0).Key().Build(),
       Fb().Rate(kCbr).Res(320, 180).S(1).Ref({0}).Upd({1}).Build(),
       Fb().Rate(kCbr).Res(640, 360).S(2).Ref({1}).Upd({2}).Build()},
      res.CallBack()));

  ASSERT_TRUE(enc->Encode(
      frame_reader->PullFrame(), {},
      {Fb().Rate(kCbr).Res(640, 360).S(2).Ref({2}).Upd({2}).Build()},
      res.CallBack()));

  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
  ASSERT_TRUE(enc->Encode(
      frame, {},
      {Fb().Rate(kCbr).Res(160, 90).S(0).Ref({0}).Upd({0}).Build(),
       Fb().Rate(kCbr).Res(320, 180).S(1).Ref({0, 1}).Upd({1}).Build(),
       Fb().Rate(kCbr).Res(640, 360).S(2).Ref({1, 2}).Upd({2}).Build()},
      res.CallBack()));

  Av1Decoder dec;
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(0))), ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(1))), ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(2))), ResolutionIs(640, 360));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(3))), ResolutionIs(640, 360));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(4))), ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(5))), ResolutionIs(320, 180));

  VideoFrame f = dec.Decode(*res.FrameAt(6));
  EXPECT_THAT(Resolution(f), ResolutionIs(640, 360));

  EXPECT_THAT(Psnr(frame, f), Gt(40));
}

TEST(LibaomAv1Encoder, InvertedTempoSpatial) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  ASSERT_TRUE(enc->Encode(
      frame_reader->PullFrame(), {},
      {Fb().Rate(kCbr).Res(320, 180).S(0).Key().Build(),
       Fb().Rate(kCbr).Res(640, 360).S(1).Ref({0}).Upd({1}).Build()},
      res.CallBack()));

  // TODO: Wait for https://aomedia-review.googlesource.com/c/aom/+/183901
  ASSERT_TRUE(enc->Encode(
      frame_reader->PullFrame(), {},
      {Fb().Rate(kCbr).Res(320, 180).S(0).Ref({0}).Upd({0}).Build()},
      res.CallBack()));

  rtc::scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
  ASSERT_TRUE(enc->Encode(
      frame, {},
      {Fb().Rate(kCbr).Res(320, 180).S(0).Ref({0}).Upd({0}).Build(),
       Fb().Rate(kCbr).Res(640, 360).S(1).Ref({1, 0}).Upd({1}).Build()},
      res.CallBack()));

  Av1Decoder dec;
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(0))), ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(1))), ResolutionIs(640, 360));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(2))), ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(3))), ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(4))), ResolutionIs(640, 360));
}

TEST(LibaomAv1Encoder, L3T1) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  Av1Decoder dec("L3T1");

  ASSERT_TRUE(enc->Encode(
      frame_reader->PullFrame(), {.effort_level = 2},
      {Fb().Rate(kCbr).Res(160, 90).S(0).Key().Build(),
       Fb().Rate(kCbr).Res(320, 180).S(1).Ref({0}).Upd({1}).Build(),
       Fb().Rate(kCbr).Res(640, 360).S(2).Ref({1}).Upd({2}).Build()},
      res.CallBack()));

  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(0))), ResolutionIs(160, 90));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(1))), ResolutionIs(320, 180));
  EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(2))), ResolutionIs(640, 360));

  for (int i = 0; i < 6; i += 3) {
    auto in_frame = frame_reader->PullFrame();
    ASSERT_TRUE(enc->Encode(
        in_frame, {},
        {Fb().Rate(kCbr).Res(160, 90).S(0).Ref({0}).Upd({0}).Build(),
         Fb().Rate(kCbr).Res(320, 180).S(1).Ref({1, 0}).Upd({1}).Build(),
         Fb().Rate(kCbr).Res(640, 360).S(2).Ref({2, 1}).Upd({2}).Build()},
        res.CallBack()));

    EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(3 + i))),
                ResolutionIs(160, 90));
    EXPECT_THAT(Resolution(dec.Decode(*res.FrameAt(4 + i))),
                ResolutionIs(320, 180));

    VideoFrame f = dec.Decode(*res.FrameAt(5 + i));
    EXPECT_THAT(Resolution(f), ResolutionIs(640, 360));
    EXPECT_THAT(Psnr(in_frame, f), Gt(40));
  }
}

TEST(LibaomAv1Encoder, ReferenceOrderDoesNotMatter) {
  auto frame_reader = CreateFrameReader();
  auto key_in = frame_reader->PullFrame();
  auto delta_in = frame_reader->PullFrame();

  std::vector<double> psnrs;
  Av1Decoder dec("ref_order");
  std::vector<int> refs = {0, 1, 2};
  do {
    auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
    EncodeResults res;
    ASSERT_TRUE(enc->Encode(
        key_in, {},
        {Fb().Rate(kCbr).Res(160, 90).S(0).Key().Build(),
         Fb().Rate(kCbr).Res(320, 180).S(1).Ref({0}).Upd({1}).Build(),
         Fb().Rate(kCbr).Res(640, 360).S(2).Ref({1}).Upd({2}).Build()},
        res.CallBack()));
    ASSERT_TRUE(enc->Encode(
        delta_in, {}, {Fb().Rate(kCbr).Res(640, 360).S(2).Ref(refs).Build()},
        res.CallBack()));

    dec.Decode(*res.FrameAt(0));
    dec.Decode(*res.FrameAt(1));
    dec.Decode(*res.FrameAt(2));
    psnrs.push_back(Psnr(delta_in, dec.Decode(*res.FrameAt(3))));
    printf("Refs {%d, %d, %d}  PSNR %f\n", refs[0], refs[1], refs[2],
           psnrs.back());
  } while (std::next_permutation(refs.begin(), refs.end()));

  std::sort(psnrs.begin(), psnrs.end());
  EXPECT_THAT(psnrs[0], Gt(40));
  EXPECT_THAT(psnrs.back() - psnrs[0], Lt(1.0));
}

TEST(LibaomAv1Encoder, L3T1_KEY) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  Av1Decoder dec_s0("L3T1_KEY_S0");
  Av1Decoder dec_s1("L3T1_KEY_S1");
  Av1Decoder dec_s2("L3T1_KEY_S2");

  ASSERT_TRUE(enc->Encode(
      frame_reader->PullFrame(), {},
      {Fb().Rate(kCbr).Res(160, 90).S(0).Key().Build(),
       Fb().Rate(kCbr).Res(320, 180).S(1).Ref({0}).Upd({1}).Build(),
       Fb().Rate(kCbr).Res(640, 360).S(2).Ref({1}).Upd({2}).Build()},
      res.CallBack()));

  EXPECT_THAT(Resolution(dec_s0.Decode(*res.FrameAt(0))),
              ResolutionIs(160, 90));

  dec_s1.Decode(*res.FrameAt(0));
  EXPECT_THAT(Resolution(dec_s1.Decode(*res.FrameAt(1))),
              ResolutionIs(320, 180));

  dec_s2.Decode(*res.FrameAt(0));
  dec_s2.Decode(*res.FrameAt(1));
  EXPECT_THAT(Resolution(dec_s2.Decode(*res.FrameAt(2))),
              ResolutionIs(640, 360));

  for (int i = 0; i < 6; i += 3) {
    ASSERT_TRUE(enc->Encode(
        frame_reader->PullFrame(), {},
        {Fb().Rate(kCbr).Res(160, 90).S(0).Ref({0}).Upd({0}).Build(),
         Fb().Rate(kCbr).Res(320, 180).S(1).Ref({1}).Upd({1}).Build(),
         Fb().Rate(kCbr).Res(640, 360).S(2).Ref({2}).Upd({2}).Build()},
        res.CallBack()));

    EXPECT_THAT(Resolution(dec_s0.Decode(*res.FrameAt(3 + i))),
                ResolutionIs(160, 90));
    EXPECT_THAT(Resolution(dec_s1.Decode(*res.FrameAt(4 + i))),
                ResolutionIs(320, 180));
    EXPECT_THAT(Resolution(dec_s2.Decode(*res.FrameAt(5 + i))),
                ResolutionIs(640, 360));
  }

  // EXPECT_THAT(Psnr(frame, f5), Gt(40));
}

TEST(LibaomAv1Encoder, S3T1) {
  auto frame_reader = CreateFrameReader();
  auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
  EncodeResults res;

  Av1Decoder dec_s0("S3T1_S0");
  Av1Decoder dec_s1("S3T1_S1");
  Av1Decoder dec_s2("S3T1_S2");

  ASSERT_TRUE(
      enc->Encode(frame_reader->PullFrame(), {},
                  {Fb().Rate(kCbr).Res(160, 90).S(0).Start().Upd({0}).Build(),
                   Fb().Rate(kCbr).Res(320, 180).S(1).Start().Upd({1}).Build(),
                   Fb().Rate(kCbr).Res(640, 360).S(2).Start().Upd({2}).Build()},
                  res.CallBack()));
  VideoFrame f0 = dec_s0.Decode(*res.FrameAt(0));
  EXPECT_THAT(Resolution(f0), ResolutionIs(160, 90));

  VideoFrame f1 = dec_s1.Decode(*res.FrameAt(1));
  EXPECT_THAT(Resolution(f1), ResolutionIs(320, 180));

  VideoFrame f2 = dec_s2.Decode(*res.FrameAt(2));
  EXPECT_THAT(Resolution(f2), ResolutionIs(640, 360));

  for (int i = 0; i < 6; i += 3) {
    ASSERT_TRUE(enc->Encode(
        frame_reader->PullFrame(), {},
        {Fb().Rate(kCbr).Res(160, 90).S(0).Ref({0}).Upd({0}).Build(),
         Fb().Rate(kCbr).Res(320, 180).S(1).Ref({1}).Upd({1}).Build(),
         Fb().Rate(kCbr).Res(640, 360).S(2).Ref({2}).Upd({2}).Build()},
        res.CallBack()));

    VideoFrame f3 = dec_s0.Decode(*res.FrameAt(3 + i));
    EXPECT_THAT(Resolution(f3), ResolutionIs(160, 90));

    VideoFrame f4 = dec_s1.Decode(*res.FrameAt(4 + i));
    EXPECT_THAT(Resolution(f4), ResolutionIs(320, 180));

    VideoFrame f5 = dec_s2.Decode(*res.FrameAt(5 + i));
    EXPECT_THAT(Resolution(f5), ResolutionIs(640, 360));
  }

  // EXPECT_THAT(Psnr(frame, f5), Gt(40));
}

TEST(LibaomAv1Encoder, HigherEffortLevelYieldsHigherQualityFrames) {
  auto frame_in = CreateFrameReader()->PullFrame();
  std::pair<int, int> effort_range = LibaomAv1EncoderFactory()
                                         .GetEncoderCapabilities()
                                         .performance.min_max_effort_level;
  // Cbr rc{.duration = TimeDelta::Millis(100),
  //       .target_bitrate = DataRate::KilobitsPerSec(100)};
  absl::optional<double> psnr_last;
  Av1Decoder dec("effort_level");

  for (int i = effort_range.first; i <= effort_range.second; ++i) {
    auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
    EncodeResults res;
    ASSERT_TRUE(enc->Encode(frame_in, {.effort_level = i},
                            {Fb().Rate(kCbr).Res(640, 360).Key().Build()},
                            res.CallBack()));
    double psnr = Psnr(frame_in, dec.Decode(*res.FrameAt(0)));
    printf("PSNR %f (%d)\n", psnr, i);
    EXPECT_THAT(psnr, Gt(psnr_last));
    psnr_last = psnr;
  }
}

TEST(LibaomAv1Encoder, BitrateConsistentAcrossSpatialLayers) {
  int max_spatial_layers = LibaomAv1EncoderFactory()
                               .GetEncoderCapabilities()
                               .prediction_constraints.max_spatial_layers;
  const Cbr kRate{.duration = TimeDelta::Millis(100),
                  .target_bitrate = DataRate::KilobitsPerSec(100)};

  for (int sid = 0; sid < max_spatial_layers; ++sid) {
    std::string wut = "cbr_sl_";
    wut += std::to_string(sid);
    Av1Decoder dec(wut);

    auto frame_reader = CreateFrameReader();
    auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCbrEncoderSettings, {});
    DataSize total_size = DataSize::Zero();
    TimeDelta total_duration = TimeDelta::Zero();
    EncodeResults res;
    ASSERT_TRUE(enc->Encode(
        frame_reader->PullFrame(), {},
        {Fb().Rate(kRate).Res(640, 360).S(sid).Key().Build()}, res.CallBack()));
    total_size += DataSize::Bytes(res.FrameAt(0)->bitstream_data->size());
    total_duration += kRate.duration;
    dec.Decode(*res.FrameAt(0));

    for (int f = 1; f < 20; ++f) {
      ASSERT_TRUE(enc->Encode(
          frame_reader->PullFrame(), {},
          {Fb().Rate(kRate).Res(640, 360).S(sid).Ref({0}).Upd({0}).Build()},
          res.CallBack()));
      total_size += DataSize::Bytes(res.FrameAt(f)->bitstream_data->size());
      total_duration += kRate.duration;
      dec.Decode(*res.FrameAt(f));
    }

    double encode_kbps = (total_size / total_duration).kbps();
    double target_kbps = kRate.target_bitrate.kbps();

    EXPECT_NEAR(encode_kbps, target_kbps, target_kbps * 0.05);
  }
}

TEST(LibaomAv1Encoder, ConstantQp) {
  int max_spatial_layers = LibaomAv1EncoderFactory()
                               .GetEncoderCapabilities()
                               .prediction_constraints.max_spatial_layers;
  constexpr int kQp = 50;
  for (int sid = 0; sid < max_spatial_layers; ++sid) {
    auto enc = LibaomAv1EncoderFactory().CreateEncoder(kCqpEncoderSettings, {});

    std::string wut = "cqp_sl_";
    wut += std::to_string(sid);
    Av1Decoder dec(wut);
    auto frame_reader = CreateFrameReader();
    EncodeResults res;
    ASSERT_TRUE(enc->Encode(
        frame_reader->PullFrame(), {},
        {Fb().Rate(Cqp{.target_qp = kQp}).Res(640, 360).S(sid).Key().Build()},
        res.CallBack()));
    EXPECT_THAT(res.FrameAt(0)->encoded_qp, Eq(kQp));
    dec.Decode(*res.FrameAt(0));

    for (int f = 1; f < 20; ++f) {
      ASSERT_TRUE(enc->Encode(frame_reader->PullFrame(), {},
                              {Fb().Rate(Cqp{.target_qp = kQp - f})
                                   .Res(640, 360)
                                   .S(sid)
                                   .Ref({0})
                                   .Upd({0})
                                   .Build()},
                              res.CallBack()));
      EXPECT_THAT(res.FrameAt(f)->encoded_qp, Eq(kQp - f));
      dec.Decode(*res.FrameAt(f));
    }
  }
}

}  // namespace
}  // namespace webrtc
