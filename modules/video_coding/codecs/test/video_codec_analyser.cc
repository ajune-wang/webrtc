/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_analyser.h"

#include <memory>

#include "api/video/i420_buffer.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_frame.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "rtc_base/time_utils.h"
#include "third_party/libyuv/include/libyuv/compare.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace webrtc {
namespace test {

namespace {
using CodingSettings = VideoCodecAnalyser::CodingSettings;

void CalcPsnr(const I420BufferInterface& ref_buffer,
              const I420BufferInterface& dec_buffer,
              double* psnr_y,
              double* psnr_u,
              double* psnr_v,
              double* psnr_yuv) {
  RTC_CHECK_EQ(ref_buffer.width(), dec_buffer.width());
  RTC_CHECK_EQ(ref_buffer.height(), dec_buffer.height());

  uint64_t sse_y = libyuv::ComputeSumSquareErrorPlane(
      dec_buffer.DataY(), dec_buffer.StrideY(), ref_buffer.DataY(),
      ref_buffer.StrideY(), dec_buffer.width(), dec_buffer.height());

  uint64_t sse_u = libyuv::ComputeSumSquareErrorPlane(
      dec_buffer.DataU(), dec_buffer.StrideU(), ref_buffer.DataU(),
      ref_buffer.StrideU(), dec_buffer.width() / 2, dec_buffer.height() / 2);

  uint64_t sse_v = libyuv::ComputeSumSquareErrorPlane(
      dec_buffer.DataV(), dec_buffer.StrideV(), ref_buffer.DataV(),
      ref_buffer.StrideV(), dec_buffer.width() / 2, dec_buffer.height() / 2);

  int num_y_samples = dec_buffer.width() * dec_buffer.height();
  *psnr_y = libyuv::SumSquareErrorToPsnr(sse_y, num_y_samples);
  *psnr_u = libyuv::SumSquareErrorToPsnr(sse_u, num_y_samples / 4);
  *psnr_v = libyuv::SumSquareErrorToPsnr(sse_v, num_y_samples / 4);
  *psnr_yuv = libyuv::SumSquareErrorToPsnr(sse_y + sse_u + sse_v,
                                           num_y_samples + num_y_samples / 2);
}

}  // namespace

VideoCodecAnalyser::VideoCodecAnalyser(
    VideoFrameProvider* reference_frame_provider)
    : reference_frame_provider_(reference_frame_provider),
      quality_processing_task_queue_("Quality processing") {}

void VideoCodecAnalyser::EncodeStarted(const VideoFrame& input_frame) {
  MutexLock lock(&stats_mutex_);

  int64_t encode_started_ns = rtc::TimeNanos();
  for (int spatial_idx = 0; spatial_idx < kMaxSpatialLayers; ++spatial_idx) {
    VideoCodecTestStats::FrameStatistics* fs =
        stats_.GetOrAddFrame(input_frame.timestamp(), spatial_idx);
    fs->encode_start_ns = encode_started_ns;
  }
}

void VideoCodecAnalyser::EncodeFinished(const EncodedImage& frame,
                                        const CodingSettings& coding_settings) {
  int64_t encode_finished_ns = rtc::TimeNanos();

  int spatial_idx = frame.SpatialIndex().value_or(0);

  MutexLock lock(&stats_mutex_);
  VideoCodecTestStats::FrameStatistics* fs =
      stats_.GetOrAddFrame(frame.Timestamp(), spatial_idx);

  fs->spatial_idx = spatial_idx;
  fs->temporal_idx = frame.TemporalIndex().value_or(0);
  fs->frame_type = frame._frameType;
  fs->qp = frame.qp_;

  fs->encode_time_us =
      (encode_finished_ns - fs->encode_start_ns) / rtc::kNumNanosecsPerMicrosec;
  fs->length_bytes = frame.size();

  fs->target_bitrate_kbps = coding_settings.bitrate_kbps;
  fs->target_framerate_fps = coding_settings.framerate_fps;
  fs->encoding_successful = true;
}

void VideoCodecAnalyser::DecodeStarted(const EncodedImage& frame) {
  MutexLock lock(&stats_mutex_);
  VideoCodecTestStats::FrameStatistics* fs =
      stats_.GetOrAddFrame(frame.Timestamp(), frame.SpatialIndex().value_or(0));
  if (fs->length_bytes == 0) {
    // In encode-decode test the frame size is set in EncodeFinished. In
    // decode-only test set it here.
    fs->length_bytes = frame.size();
  }
  fs->decode_start_ns = rtc::TimeNanos();
}

void VideoCodecAnalyser::DecodeFinished(const VideoFrame& frame,
                                        int spatial_idx) {
  int64_t decode_finished_ns = rtc::TimeNanos();
  {
    MutexLock lock(&stats_mutex_);
    VideoCodecTestStats::FrameStatistics* fs =
        stats_.GetFrameWithTimestamp(frame.timestamp(), spatial_idx);

    fs->decode_time_us = (decode_finished_ns - fs->decode_start_ns) /
                         rtc::kNumNanosecsPerMicrosec;
    fs->decoded_width = frame.width();
    fs->decoded_height = frame.height();

    fs->decoding_successful = true;
  }

  if (reference_frame_provider_ != nullptr) {
    // Run quality analyses on a separate thread to not block encoding or/and
    // decoding. Hardware decoders may have limited number of output buffers
    // and may drop input frames if all output buffers are occupied. Copy
    // decoded pixels into a local buffer to release decoded video frame and
    // free decoder's output buffer.
    auto decoded_buffer = I420Buffer::Copy(*frame.video_frame_buffer());

    uint32_t timestamp_rtp = frame.timestamp();
    quality_processing_task_queue_.PostTask(
        [this, decoded_buffer, timestamp_rtp, spatial_idx]() {
          auto ref_frame = reference_frame_provider_->GetFrame(timestamp_rtp);
          RTC_CHECK(ref_frame != nullptr);

          auto ref_buffer = ref_frame->video_frame_buffer()->ToI420();
          if (ref_buffer->width() != decoded_buffer->width() ||
              ref_buffer->height() != decoded_buffer->height()) {
            ref_buffer = ScaleVideoFrameBuffer(
                *ref_buffer, decoded_buffer->width(), decoded_buffer->height());
          }

          double psnr_y;
          double psnr_v;
          double psnr_u;
          double psnr_yuv;
          CalcPsnr(*decoded_buffer, *ref_buffer, &psnr_y, &psnr_u, &psnr_v,
                   &psnr_yuv);

          {
            MutexLock lock(&stats_mutex_);
            VideoCodecTestStats::FrameStatistics* fs =
                this->stats_.GetFrameWithTimestamp(timestamp_rtp, spatial_idx);
            fs->psnr_y = static_cast<float>(psnr_y);
            fs->psnr_u = static_cast<float>(psnr_u);
            fs->psnr_v = static_cast<float>(psnr_v);
            fs->psnr = static_cast<float>(psnr_yuv);

            fs->quality_analysis_successful = true;
          }
        });
  }
}

void VideoCodecAnalyser::FinishAnalysis() {
  quality_processing_task_queue_.WaitForPreviouslyPostedTasks();
}

std::unique_ptr<VideoCodecTestStats> VideoCodecAnalyser::GetStats() {
  MutexLock lock(&stats_mutex_);
  return std::make_unique<VideoCodecTestStatsImpl>(stats_);
}

VideoCodecTestStats::FrameStatistics* VideoCodecAnalyser::GetFrame(
    uint32_t timestamp_rtp,
    int spatial_idx) {
  MutexLock lock(&stats_mutex_);
  return stats_.GetFrameWithTimestamp(timestamp_rtp, spatial_idx);
}

}  // namespace test
}  // namespace webrtc
