/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "absl/types/optional.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/event.h"
#include "test/frame_generator.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/video_frame_reader.h"
#include "test/video_codec_settings.h"

namespace webrtc {
namespace test {
namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 240;
constexpr int kVideoFramesCount = 30;
constexpr int kMaxFramerate = 30;
constexpr int kMaxFrameEncodeWaitTimeoutMs = 2000;
static const VideoEncoder::Capabilities kCapabilities(false);

class IvfFileWriterEncodedCallback : public EncodedImageCallback {
 public:
  IvfFileWriterEncodedCallback(const std::string& file_name,
                               VideoCodecType video_codec_type,
                               int expected_frames_count)
      : file_writer_(
            IvfFileWriter::Wrap(FileWrapper::OpenWriteOnly(file_name), 0)),
        video_codec_type_(video_codec_type),
        expected_frames_count_(expected_frames_count) {
    EXPECT_TRUE(file_writer_.get());
  }
  ~IvfFileWriterEncodedCallback() { EXPECT_TRUE(file_writer_->Close()); }

  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info,
                        const RTPFragmentationHeader* fragmentation) override {
    EXPECT_TRUE(file_writer_->WriteFrame(encoded_image, video_codec_type_));

    rtc::CritScope crit(&lock_);
    received_frames_count_++;
    RTC_CHECK_LE(received_frames_count_, expected_frames_count_);
    if (received_frames_count_ == expected_frames_count_) {
      expected_frames_count_received_.Set();
    }
    return Result(Result::Error::OK);
  }

  bool WaitForExpectedFramesReceived(int timeout_ms) {
    return expected_frames_count_received_.Wait(timeout_ms);
  }

 private:
  std::unique_ptr<IvfFileWriter> file_writer_;
  const VideoCodecType video_codec_type_;
  const int expected_frames_count_;

  rtc::CriticalSection lock_;
  int received_frames_count_ RTC_GUARDED_BY(lock_) = 0;
  rtc::Event expected_frames_count_received_;
};

class IvfVideoFrameReaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    file_name_ =
        webrtc::test::TempFilename(webrtc::test::OutputPath(), "test_file.ivf");
  }
  void TearDown() override { webrtc::test::RemoveFile(file_name_); }

  void CreateTestVideoFile(VideoCodecType video_codec_type,
                           std::unique_ptr<VideoEncoder> video_encoder) {
    std::unique_ptr<test::FrameGenerator> frame_generator =
        test::FrameGenerator::CreateSquareGenerator(
            kWidth, kHeight, test::FrameGenerator::OutputType::kI420,
            absl::nullopt);

    VideoCodec codec_settings;
    webrtc::test::CodecSettings(video_codec_type, &codec_settings);
    codec_settings.width = kWidth;
    codec_settings.height = kHeight;
    codec_settings.maxFramerate = kMaxFramerate;

    IvfFileWriterEncodedCallback ivf_writer_callback(
        file_name_, video_codec_type, kVideoFramesCount);

    video_encoder->RegisterEncodeCompleteCallback(&ivf_writer_callback);
    ASSERT_EQ(WEBRTC_VIDEO_CODEC_OK,
              video_encoder->InitEncode(
                  &codec_settings,
                  VideoEncoder::Settings(kCapabilities, /*number_of_cores=*/1,
                                         /*max_payload_size=*/0)));

    uint32_t last_frame_timestamp = 0;

    for (int i = 0; i < kVideoFramesCount; ++i) {
      VideoFrame* frame = frame_generator->NextFrame();
      const uint32_t timestamp =
          last_frame_timestamp +
          kVideoPayloadTypeFrequency / codec_settings.maxFramerate;
      frame->set_timestamp(timestamp);

      last_frame_timestamp = timestamp;

      ASSERT_EQ(WEBRTC_VIDEO_CODEC_OK, video_encoder->Encode(*frame, nullptr));
      video_frames_.push_back(*frame);
    }

    ASSERT_TRUE(ivf_writer_callback.WaitForExpectedFramesReceived(
        kMaxFrameEncodeWaitTimeoutMs));
  }

  std::string file_name_;
  std::vector<VideoFrame> video_frames_;
};

}  // namespace

TEST_F(IvfVideoFrameReaderTest, Vp8) {
  CreateTestVideoFile(VideoCodecType::kVideoCodecVP8, VP8Encoder::Create());
  IvfVideoFrameReader reader(file_name_);
  EXPECT_EQ(reader.GetFramesCount(), video_frames_.size());
  for (size_t i = 0; i < reader.GetFramesCount(); ++i) {
    auto& expected_frame = video_frames_[i];
    absl::optional<VideoFrame> actual_frame = reader.ReadFrame();
    EXPECT_TRUE(actual_frame);
    EXPECT_GT(I420PSNR(&expected_frame, &actual_frame.value()), 38);
  }
  reader.Close();
}

TEST_F(IvfVideoFrameReaderTest, Vp8DoubleRead) {
  CreateTestVideoFile(VideoCodecType::kVideoCodecVP8, VP8Encoder::Create());
  IvfVideoFrameReader reader(file_name_);
  EXPECT_EQ(reader.GetFramesCount(), video_frames_.size());
  for (size_t i = 0; i < reader.GetFramesCount() * 2; ++i) {
    auto& expected_frame = video_frames_[i % video_frames_.size()];
    absl::optional<VideoFrame> actual_frame = reader.ReadFrame();
    EXPECT_TRUE(actual_frame);
    EXPECT_GT(I420PSNR(&expected_frame, &actual_frame.value()), 38);
  }
  reader.Close();
}

TEST_F(IvfVideoFrameReaderTest, Vp9) {
  CreateTestVideoFile(VideoCodecType::kVideoCodecVP9, VP9Encoder::Create());
  IvfVideoFrameReader reader(file_name_);
  EXPECT_EQ(reader.GetFramesCount(), video_frames_.size());
  for (size_t i = 0; i < reader.GetFramesCount(); ++i) {
    auto& expected_frame = video_frames_[i];
    absl::optional<VideoFrame> actual_frame = reader.ReadFrame();
    EXPECT_TRUE(actual_frame);
    EXPECT_GT(I420PSNR(&expected_frame, &actual_frame.value()), 38);
  }
  reader.Close();
}

TEST_F(IvfVideoFrameReaderTest, H264) {
  CreateTestVideoFile(
      VideoCodecType::kVideoCodecH264,
      H264Encoder::Create(cricket::VideoCodec(cricket::kH264CodecName)));
  IvfVideoFrameReader reader(file_name_);
  EXPECT_EQ(reader.GetFramesCount(), video_frames_.size());
  for (size_t i = 0; i < reader.GetFramesCount(); ++i) {
    auto& expected_frame = video_frames_[i];
    absl::optional<VideoFrame> actual_frame = reader.ReadFrame();
    EXPECT_TRUE(actual_frame);
    EXPECT_GT(I420PSNR(&expected_frame, &actual_frame.value()), 38);
  }
  reader.Close();
}

}  // namespace test
}  // namespace webrtc
