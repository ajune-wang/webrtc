/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_codec.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/test/video_codec_tester.h"
#include "api/test/videocodec_test_stats.h"
#include "api/units/data_rate.h"
#include "api/units/frequency.h"
#include "api/video/i420_buffer.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/media_constants.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/codecs/test/video_codec_tester_impl.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::Combine;
using ::testing::Values;

struct VideoInfo {
  std::string name;
  int width;
  int height;
};

struct CodecInfo {
  std::string type;
  std::string encoder;
  std::string decoder;
};

struct CodingSettings {
  ScalabilityMode scalability_mode = ScalabilityMode::kL1T1;

  // Frame rate of top temporal layer.
  Frequency framerate = Frequency::Hertz(30);

  // Resolution of spatial layers, from top to low layer.
  std::vector<int> width = {352};
  std::vector<int> height = {288};

  // Bitrate of spatial and temporal layers, from top to low layer.
  // For example: L1T1 -> L1T0 -> L0T1 -> L0T0.
  std::vector<DataRate> bitrate = {DataRate::KilobitsPerSec(512)};
};

using FrameSettings = std::map<size_t, CodingSettings>;

const VideoInfo kForemanCif =
    VideoInfo({.name = "foreman_cif", .width = 352, .height = 288});

const CodecInfo kLibvpxVp8 =
    CodecInfo({.type = "VP8", .encoder = "libvpx", .decoder = "libvpx"});

const FrameSettings kL1T130Fps512Kbps = {
    {0, CodingSettings({.scalability_mode = ScalabilityMode::kL1T1,
                        .framerate = Frequency::Hertz(30),
                        .bitrate = {DataRate::KilobitsPerSec(512)}})}};

class LocalFrameReader : public VideoCodecTester::TestFrameReader {
 public:
  LocalFrameReader(std::unique_ptr<VideoFrameReader> frame_reader,
                   const FrameSettings& frame_settings,
                   size_t num_frames)
      : frame_reader_(std::move(frame_reader)),
        frame_settings_(frame_settings),
        num_frames_(num_frames),
        frame_num_(0),
        timestamp_rtp_(0) {
    // Ensure settings for the first frame is provided.
    RTC_CHECK_GT(frame_settings_.size(), 0u);
    RTC_CHECK_EQ(frame_settings_.begin()->first, 0);
  }

  // Reads and returns `VideoFrame` with monotonically increasing timestamps.
  absl::optional<VideoFrame> PullFrame() override {
    if (frame_num_ >= num_frames_) {
      // End of stream.
      return absl::nullopt;
    }

    CodingSettings frame_settings =
        std::prev(frame_settings_.upper_bound(frame_num_))->second;
    auto frame = frame_reader_->ReadFrame(frame_num_);
    ++frame_num_;

    frame->set_timestamp(timestamp_rtp_);
    timestamp_rtp_ += Frequency::KiloHertz(90) / frame_settings.framerate;

    return frame;
  }

  // Reads and returns `VideoFrame` from position specified by `frame_num`.
  // Frame timestamp is not set.
  absl::optional<VideoFrame> ReadFrame(size_t frame_num) override {
    return frame_reader_->ReadFrame(frame_num);
  }

  void Close() override { frame_reader_->Close(); }

 protected:
  std::unique_ptr<VideoFrameReader> frame_reader_;
  const FrameSettings& frame_settings_;
  size_t num_frames_;
  size_t frame_num_;
  uint32_t timestamp_rtp_;
};

class LocalTestEncoder : public VideoCodecTester::TestEncoder,
                         public EncodedImageCallback {
 public:
  LocalTestEncoder(std::unique_ptr<VideoEncoder> encoder,
                   const CodecInfo& codec_info,
                   const FrameSettings& frame_settings)
      : encoder_(std::move(encoder)),
        codec_info_(codec_info),
        frame_settings_(frame_settings),
        frame_num_(0) {
    // Ensure settings for the first frame is provided.
    RTC_CHECK_GT(frame_settings_.size(), 0u);
    RTC_CHECK_EQ(frame_settings_.begin()->first, 0);

    encoder_->RegisterEncodeCompleteCallback(this);
  }

  void Encode(const VideoFrame& frame, EncodeCallback callback) override {
    callbacks_[frame.timestamp()] = std::move(callback);

    if (auto it = frame_settings_.find(frame_num_);
        it != frame_settings_.end()) {
      Configure(it->second);
    }

    int result = encoder_->Encode(frame, nullptr);
    RTC_CHECK_EQ(result, WEBRTC_VIDEO_CODEC_OK);
    ++frame_num_;
  }

 protected:
  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info) override {
    if (auto it = callbacks_.find(encoded_image.Timestamp());
        it != callbacks_.end()) {
      VideoCodecTester::FrameSettings frame_settings;
      it->second(encoded_image, frame_settings);
      // TODO(ssilkin): Remove callbacks for older frames.
    }
    return Result(Result::Error::OK);
  }

  void Configure(const CodingSettings& settings) {
    VideoCodec vc;
    vc.width = settings.width[0];
    vc.height = settings.height[0];
    vc.startBitrate = settings.bitrate[0].kbps();
    vc.maxBitrate = settings.bitrate[0].kbps();
    vc.minBitrate = 0;
    vc.maxFramerate = static_cast<uint32_t>(settings.framerate.hertz());
    vc.active = true;
    vc.qpMax = 0;
    vc.numberOfSimulcastStreams = 0;
    vc.mode = webrtc::VideoCodecMode::kRealtimeVideo;
    vc.SetFrameDropEnabled(false);

    vc.codecType = PayloadStringToCodecType(codec_info_.type);
    if (vc.codecType == kVideoCodecVP8) {
      *(vc.VP8()) = VideoEncoder::GetDefaultVp8Settings();
    } else if (vc.codecType == kVideoCodecVP9) {
      *(vc.VP9()) = VideoEncoder::GetDefaultVp9Settings();
    } else if (vc.codecType == kVideoCodecH264) {
      *(vc.H264()) = VideoEncoder::GetDefaultH264Settings();
    }

    VideoEncoder::Settings es(
        VideoEncoder::Capabilities(/*loss_notification=*/false),
        /*number_of_cores=*/1,
        /*max_payload_size=*/1440);

    int result = encoder_->InitEncode(&vc, es);
    RTC_CHECK_EQ(result, WEBRTC_VIDEO_CODEC_OK);
  }

  std::unique_ptr<VideoEncoder> encoder_;
  const CodecInfo& codec_info_;
  const FrameSettings& frame_settings_;
  int frame_num_;
  std::map<uint32_t, EncodeCallback> callbacks_;
};

class LocalTestDecoder : public VideoCodecTester::TestDecoder,
                         public DecodedImageCallback {
 public:
  LocalTestDecoder(std::unique_ptr<VideoDecoder> decoder,
                   const CodecInfo& codec_info)
      : decoder_(std::move(decoder)), codec_info_(codec_info), frame_num_(0) {
    decoder_->RegisterDecodeCompleteCallback(this);
  }
  void Decode(const EncodedImage& frame, DecodeCallback callback) override {
    callbacks_[frame.Timestamp()] = std::move(callback);

    if (frame_num_ == 0) {
      Configure();
    }

    decoder_->Decode(frame, /*missing_frames=*/false,
                     /*render_time_ms=*/0);
    ++frame_num_;
  }

  void Configure() {
    VideoDecoder::Settings ds;
    ds.set_codec_type(PayloadStringToCodecType(codec_info_.type));
    ds.set_number_of_cores(1);

    bool result = decoder_->Configure(ds);
    RTC_CHECK(result);
  }

 protected:
  int Decoded(VideoFrame& decoded_frame) override {
    if (auto it = callbacks_.find(decoded_frame.timestamp());
        it != callbacks_.end()) {
      it->second(decoded_frame);
      // TODO(ssilkin): Remove callbacks for older frames.
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }

  std::unique_ptr<VideoDecoder> decoder_;
  const CodecInfo& codec_info_;
  int frame_num_;
  std::map<uint32_t, DecodeCallback> callbacks_;
};

std::unique_ptr<VideoCodecTester::TestEncoder> CreateEncoder(
    const CodecInfo& codec_info,
    const FrameSettings& frame_settings) {
  auto factory = CreateBuiltinVideoEncoderFactory();
  auto encoder = factory->CreateVideoEncoder(SdpVideoFormat(codec_info.type));
  return std::make_unique<LocalTestEncoder>(std::move(encoder), codec_info,
                                            frame_settings);
}

std::unique_ptr<VideoCodecTester::TestDecoder> CreateDecoder(
    const CodecInfo& codec_info) {
  auto factory = CreateBuiltinVideoDecoderFactory();
  auto decoder = factory->CreateVideoDecoder(SdpVideoFormat(codec_info.type));
  return std::make_unique<LocalTestDecoder>(std::move(decoder), codec_info);
}

}  // namespace

class EncodeDecodeTest
    : public ::testing::TestWithParam<
          std::tuple<VideoInfo, CodecInfo, FrameSettings, size_t>> {};

TEST_P(EncodeDecodeTest, TestEncodeDecode) {
  VideoInfo video_info = std::get<0>(GetParam());
  CodecInfo codec_info = std::get<1>(GetParam());
  FrameSettings frame_settings = std::get<2>(GetParam());
  size_t num_frames = std::get<3>(GetParam());

  auto frame_reader =
      std::make_unique<YuvFrameReaderImpl>(ResourcePath(video_info.name, "yuv"),
                                           video_info.width, video_info.height);
  frame_reader->Init();
  auto test_frame_reader = std::make_unique<LocalFrameReader>(
      std::move(frame_reader), frame_settings, num_frames);

  auto encoder = CreateEncoder(codec_info, frame_settings);
  auto decoder = CreateDecoder(codec_info);

  VideoCodecTesterImpl tester;

  VideoCodecTester::TestSettings test_settings;
  auto stats =
      tester.RunEncodeDecodeTest(std::move(test_frame_reader), test_settings,
                                 std::move(encoder), std::move(decoder));

  auto frame = stats->GetFrameStatistics();
  printf("\n%s", frame[0].ToString().c_str());
  printf("\n%s", frame[1].ToString().c_str());
}

INSTANTIATE_TEST_SUITE_P(All,
                         EncodeDecodeTest,
                         Combine(
                             /*video=*/Values(kForemanCif),
                             /*codec=*/Values(kLibvpxVp8),
                             /*frame_settings=*/Values(kL1T130Fps512Kbps),
                             /*num_frames=*/Values(2)));

}  // namespace test

}  // namespace webrtc
