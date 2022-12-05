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
#include "api/test/create_video_codec_tester.h"
#include "api/test/videocodec_test_stats.h"
#include "api/units/data_rate.h"
#include "api/units/frequency.h"
#include "api/video/i420_buffer.h"
#include "api/video/resolution.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/base/media_constants.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::Combine;
using ::testing::Values;

using Layer = std::pair<int, int>;

struct VideoInfo {
  std::string name;
  Resolution resolution;
};

struct CodecInfo {
  std::string type;
  std::string encoder;
  std::string decoder;
};

struct CodingSettings {
  ScalabilityMode scalability_mode = ScalabilityMode::kL1T1;

  // Spatial layer resolution.
  std::map<int, Resolution> resolution = {{0, {.width = 352, .height = 288}}};

  // Top temporal layer frame rate.
  Frequency framerate = Frequency::Hertz(30);

  // Spatial/temporal layer bitrate.
  std::map<Layer, DataRate> bitrate = {
      {Layer(0, 0), DataRate::KilobitsPerSec(512)}};
};

using FrameSettings = std::map<int, CodingSettings>;

struct TestSettings {
  int num_frames = 1;
  FrameSettings frame_settings;
};

constexpr Frequency k90kHz = Frequency::Hertz(90000);

const VideoInfo kFourPeople_1280x720_30 =
    VideoInfo({.name = "FourPeople_1280x720_30",
               .resolution = {.width = 1280, .height = 720}});

const CodecInfo kLibvpxVp8 =
    CodecInfo({.type = "VP8", .encoder = "libvpx", .decoder = "libvpx"});

const TestSettings kFramerateHighLowHigh = {
    .num_frames = 50,
    .frame_settings = {
        {/*frame_num=*/0,
         CodingSettings(
             {.scalability_mode = ScalabilityMode::kL1T1,
              .resolution = {{0, {.width = 352, .height = 288}}},
              .framerate = Frequency::Hertz(30),
              .bitrate = {{Layer(0, 0), DataRate::KilobitsPerSec(200)}}})},
        {/*frame_num=*/10,
         CodingSettings(
             {.scalability_mode = ScalabilityMode::kL1T1,
              .resolution = {{0, {.width = 352, .height = 288}}},
              .framerate = Frequency::Hertz(15),
              .bitrate = {{Layer(0, 0), DataRate::KilobitsPerSec(200)}}})},
        {/*frame_num=*/20,
         CodingSettings(
             {.scalability_mode = ScalabilityMode::kL1T1,
              .resolution = {{0, {.width = 352, .height = 288}}},
              .framerate = Frequency::Hertz(7.5),
              .bitrate = {{Layer(0, 0), DataRate::KilobitsPerSec(200)}}})},
        {/*frame_num=*/30,
         CodingSettings(
             {.scalability_mode = ScalabilityMode::kL1T1,
              .resolution = {{0, {.width = 352, .height = 288}}},
              .framerate = Frequency::Hertz(15),
              .bitrate = {{Layer(0, 0), DataRate::KilobitsPerSec(200)}}})},
        {/*frame_num=*/40,
         CodingSettings(
             {.scalability_mode = ScalabilityMode::kL1T1,
              .resolution = {{0, {.width = 352, .height = 288}}},
              .framerate = Frequency::Hertz(30),
              .bitrate = {{Layer(0, 0), DataRate::KilobitsPerSec(200)}}})}}};

class TestVideoSource : public VideoCodecTester::RawVideoSource {
 public:
  TestVideoSource(std::unique_ptr<FrameReader> frame_reader,
                  const TestSettings& test_settings)
      : frame_reader_(std::move(frame_reader)),
        test_settings_(test_settings),
        frame_num_(0),
        timestamp_rtp_(0) {
    // Ensure settings for the first frame are provided.
    RTC_CHECK_GT(test_settings_.frame_settings.size(), 0u);
    RTC_CHECK_EQ(test_settings_.frame_settings.begin()->first, 0);
  }

  // Reads and returns next frame.
  absl::optional<VideoFrame> PullFrame() override {
    if (frame_num_ >= test_settings_.num_frames) {
      // End of stream.
      return absl::nullopt;
    }

    CodingSettings cs =
        std::prev(test_settings_.frame_settings.upper_bound(frame_num_))
            ->second;

    int pulled_frame;
    auto buffer = frame_reader_->PullFrame(
        &pulled_frame, cs.resolution.rbegin()->second,
        {.num = 30, .den = static_cast<int>(cs.framerate.hertz())});
    auto frame = VideoFrame::Builder()
                     .set_video_frame_buffer(buffer)
                     .set_timestamp_rtp(timestamp_rtp_)
                     .build();

    pulled_frames_[timestamp_rtp_] = pulled_frame;

    ++frame_num_;
    timestamp_rtp_ += k90kHz / cs.framerate;

    return frame;
  }

  // Reads and returns frame specified by `frame_num`.
  VideoFrame GetFrame(uint32_t timestamp_rtp, Resolution resolution) override {
    RTC_CHECK(pulled_frames_.find(timestamp_rtp) != pulled_frames_.end())
        << "Frame with RTP timestamp " << timestamp_rtp
        << " was not pulled before";
    auto buffer =
        frame_reader_->ReadFrame(pulled_frames_[timestamp_rtp], resolution);
    return VideoFrame::Builder()
        .set_video_frame_buffer(buffer)
        .set_timestamp_rtp(timestamp_rtp)
        .build();
  }

 protected:
  std::unique_ptr<FrameReader> frame_reader_;
  const TestSettings& test_settings_;
  int frame_num_;
  uint32_t timestamp_rtp_;
  std::map<uint32_t, int> pulled_frames_;
};

class TestEncoder : public VideoCodecTester::Encoder,
                    public EncodedImageCallback {
 public:
  TestEncoder(std::unique_ptr<VideoEncoder> encoder,
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

    if (auto fs = frame_settings_.find(frame_num_);
        fs != frame_settings_.end()) {
      if (fs == frame_settings_.begin() ||
          ConfigChanged(fs->second, std::prev(fs)->second)) {
        Configure(fs->second);
      }
      if (fs == frame_settings_.begin() ||
          RatesChanged(fs->second, std::prev(fs)->second)) {
        SetRates(fs->second);
      }
    }

    int result = encoder_->Encode(frame, nullptr);
    RTC_CHECK_EQ(result, WEBRTC_VIDEO_CODEC_OK);
    ++frame_num_;
  }

 protected:
  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info) override {
    auto cb = callbacks_.find(encoded_image.Timestamp());
    RTC_CHECK(cb != callbacks_.end());
    cb->second(encoded_image);

    if (cb != callbacks_.begin()) {
      callbacks_.erase(callbacks_.begin(), cb);
    }
    return Result(Result::Error::OK);
  }

  void Configure(const CodingSettings& cs) {
    VideoCodec vc;
    vc.width = cs.resolution.at(0).width;
    vc.height = cs.resolution.at(0).height;
    vc.startBitrate = cs.bitrate.rbegin()->second.kbps();
    vc.maxBitrate = cs.bitrate.rbegin()->second.kbps();
    vc.minBitrate = 0;
    vc.maxFramerate = static_cast<uint32_t>(cs.framerate.hertz());
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

  void SetRates(const CodingSettings& cs) {
    VideoEncoder::RateControlParameters rc;
    int num_spatial_layers =
        ScalabilityModeToNumSpatialLayers(cs.scalability_mode);
    int num_temporal_layers =
        ScalabilityModeToNumSpatialLayers(cs.scalability_mode);
    for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
      for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
        RTC_CHECK(cs.bitrate.find(Layer(sidx, tidx)) != cs.bitrate.end())
            << "Bitrate for layer S=" << sidx << " T=" << tidx
            << " is not provided";
        rc.bitrate.SetBitrate(sidx, tidx,
                              cs.bitrate.at(Layer(sidx, tidx)).bps());
      }
    }
    // TODO(ssilkin): Change double to Frequency in RateControlParameters.
    rc.framerate_fps = cs.framerate.millihertz() / 1000.0;
    encoder_->SetRates(rc);
  }

  bool ConfigChanged(const CodingSettings& cs,
                     const CodingSettings& prev_cs) const {
    if (cs.scalability_mode != prev_cs.scalability_mode) {
      return true;
    }
    if (cs.resolution != prev_cs.resolution) {
      return true;
    }
    return false;
  }

  bool RatesChanged(const CodingSettings& cs,
                    const CodingSettings& prev_cs) const {
    return cs.bitrate != prev_cs.bitrate || cs.framerate != prev_cs.framerate;
  }

  std::unique_ptr<VideoEncoder> encoder_;
  const CodecInfo& codec_info_;
  const FrameSettings& frame_settings_;
  int frame_num_;
  std::map<uint32_t, EncodeCallback> callbacks_;
};

class TestDecoder : public VideoCodecTester::Decoder,
                    public DecodedImageCallback {
 public:
  TestDecoder(std::unique_ptr<VideoDecoder> decoder,
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
    auto cb = callbacks_.find(decoded_frame.timestamp());
    RTC_CHECK(cb != callbacks_.end());
    cb->second(decoded_frame);

    if (cb != callbacks_.begin()) {
      callbacks_.erase(callbacks_.begin(), cb);
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }

  std::unique_ptr<VideoDecoder> decoder_;
  const CodecInfo& codec_info_;
  int frame_num_;
  std::map<uint32_t, DecodeCallback> callbacks_;
};

std::unique_ptr<VideoCodecTester::Encoder> CreateEncoder(
    const CodecInfo& codec_info,
    const FrameSettings& frame_settings) {
  auto factory = CreateBuiltinVideoEncoderFactory();
  auto encoder = factory->CreateVideoEncoder(SdpVideoFormat(codec_info.type));
  return std::make_unique<TestEncoder>(std::move(encoder), codec_info,
                                       frame_settings);
}

std::unique_ptr<VideoCodecTester::Decoder> CreateDecoder(
    const CodecInfo& codec_info) {
  auto factory = CreateBuiltinVideoDecoderFactory();
  auto decoder = factory->CreateVideoDecoder(SdpVideoFormat(codec_info.type));
  return std::make_unique<TestDecoder>(std::move(decoder), codec_info);
}

}  // namespace

class EncodeDecodeTest : public ::testing::TestWithParam<
                             std::tuple<VideoInfo, CodecInfo, TestSettings>> {};

TEST_P(EncodeDecodeTest, DISABLED_TestEncodeDecode) {
  VideoInfo video_info = std::get<0>(GetParam());
  CodecInfo codec_info = std::get<1>(GetParam());
  TestSettings test_settings = std::get<2>(GetParam());
  FrameSettings& frame_settings = test_settings.frame_settings;

  auto frame_reader = CreateYuvFrameReader(ResourcePath(video_info.name, "yuv"),
                                           video_info.resolution);
  auto video_source =
      std::make_unique<TestVideoSource>(std::move(frame_reader), test_settings);

  std::unique_ptr<VideoCodecTester::Encoder> encoder =
      CreateEncoder(codec_info, frame_settings);
  std::unique_ptr<VideoCodecTester::Decoder> decoder =
      CreateDecoder(codec_info);

  std::unique_ptr<VideoCodecTester> tester = CreateVideoCodecTester();

  VideoCodecTester::EncodeSettings encode_settings;
  VideoCodecTester::DecodeSettings decode_settings;

  std::unique_ptr<VideoCodecTestStats> stats = tester->RunEncodeDecodeTest(
      std::move(video_source), std::move(encoder), std::move(decoder),
      encode_settings, decode_settings);

  for (auto fs = frame_settings.begin(); fs != frame_settings.end(); ++fs) {
    int first_frame = fs->first;
    int last_frame = std::next(fs) != frame_settings.end()
                         ? std::next(fs)->first - 1
                         : test_settings.num_frames - 1;

    const CodingSettings& cs = fs->second;
    auto metrics = stats->CalcVideoStatistic(
        first_frame, last_frame, cs.bitrate.rbegin()->second, cs.framerate);

    EXPECT_LE(metrics.avg_bitrate_mismatch_pct, 10);
    EXPECT_GE(metrics.avg_bitrate_mismatch_pct, -10);
  }
}

INSTANTIATE_TEST_SUITE_P(FramerateAdaptation,
                         EncodeDecodeTest,
                         Combine(
                             /*video=*/Values(kFourPeople_1280x720_30),
                             /*codec=*/Values(kLibvpxVp8),
                             /*test_settings=*/Values(kFramerateHighLowHigh)));

}  // namespace test

}  // namespace webrtc
