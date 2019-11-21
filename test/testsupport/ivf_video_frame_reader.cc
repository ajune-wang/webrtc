/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/video_frame_reader.h"

#include "api/video/encoded_image.h"
#include "api/video_codecs/video_codec.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/file_wrapper.h"

namespace webrtc {
namespace test {
namespace {

constexpr int kMaxNextFrameWaitTemeoutMs = 1000;

}  // namespace

IvfVideoFrameReader::IvfVideoFrameReader(const std::string& file_name)
    : file_reader_(
          IvfFileReader::Create(FileWrapper::OpenReadOnly(file_name))) {
  video_decoder_ = CreateVideoDecoder(file_reader_->GetVideoCodecType());
  RTC_CHECK(video_decoder_) << "No decoder found for file's video codec type";
  VideoCodec codec_settings;
  codec_settings.codecType = file_reader_->GetVideoCodecType();
  codec_settings.width = file_reader_->GetFrameWidth();
  codec_settings.height = file_reader_->GetFrameHeight();
  RTC_CHECK_EQ(video_decoder_->RegisterDecodeCompleteCallback(this),
               WEBRTC_VIDEO_CODEC_OK);
  RTC_CHECK_EQ(
      video_decoder_->InitDecode(&codec_settings, /*number_of_cores=*/1),
      WEBRTC_VIDEO_CODEC_OK);
}
IvfVideoFrameReader::~IvfVideoFrameReader() {
  Close();
}

absl::optional<VideoFrame> IvfVideoFrameReader::ReadFrame() {
  next_frame_decoded_.Reset();
  {
    rtc::CritScope crit(&lock_);
    if (!file_reader_) {
      return absl::nullopt;
    }
    absl::optional<EncodedImage> image = file_reader_->NextFrame();
    if (!image) {
      if (file_reader_->HasError()) {
        return absl::nullopt;
      }
      file_reader_->Reset();
      image = file_reader_->NextFrame();
      if (!image) {
        return absl::nullopt;
      }
    }
    RTC_DCHECK(image);
    // Last parameter is undocumented and there is no usage of it found.
    RTC_DCHECK_EQ(WEBRTC_VIDEO_CODEC_OK,
                  video_decoder_->Decode(*image, /*missing_frames=*/false,
                                         /*render_time_ms=*/0));
  }
  bool decoded = next_frame_decoded_.Wait(kMaxNextFrameWaitTemeoutMs);
  RTC_CHECK(decoded) << "Failed to decode next frame in "
                     << kMaxNextFrameWaitTemeoutMs << "ms. Can't continue";

  rtc::CritScope crit(&lock_);
  return next_frame_;
}

size_t IvfVideoFrameReader::GetFramesCount() const {
  rtc::CritScope crit(&lock_);
  return file_reader_->GetFramesCount();
}

void IvfVideoFrameReader::Close() {
  rtc::CritScope crit(&lock_);
  if (!file_reader_) {
    return;
  }
  file_reader_->Close();
  file_reader_.reset();
  // Reset decoder to prevent it from async access to |this|.
  video_decoder_.reset();
  next_frame_ = absl::nullopt;
  // Set event in case another thread is waiting on it.
  next_frame_decoded_.Set();
}

int32_t IvfVideoFrameReader::Decoded(VideoFrame& decoded_image) {
  Decoded(decoded_image, 0, 0);
  return WEBRTC_VIDEO_CODEC_OK;
}
int32_t IvfVideoFrameReader::Decoded(VideoFrame& decoded_image,
                                     int64_t decode_time_ms) {
  Decoded(decoded_image, decode_time_ms, 0);
  return WEBRTC_VIDEO_CODEC_OK;
}
void IvfVideoFrameReader::Decoded(VideoFrame& decoded_image,
                                  absl::optional<int32_t> decode_time_ms,
                                  absl::optional<uint8_t> qp) {
  rtc::CritScope crit(&lock_);
  next_frame_ = decoded_image;
  next_frame_decoded_.Set();
}

std::unique_ptr<VideoDecoder> IvfVideoFrameReader::CreateVideoDecoder(
    VideoCodecType codec_type) {
  if (codec_type == VideoCodecType::kVideoCodecVP8) {
    return VP8Decoder::Create();
  }
  if (codec_type == VideoCodecType::kVideoCodecVP9) {
    return VP9Decoder::Create();
  }
  if (codec_type == VideoCodecType::kVideoCodecH264) {
    return H264Decoder::Create();
  }
  return nullptr;
}

}  // namespace test
}  // namespace webrtc
