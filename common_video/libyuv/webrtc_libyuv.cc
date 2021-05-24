/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/libyuv/include/webrtc_libyuv.h"

#include <cstdint>

#include "api/video/i420_buffer.h"
#include "common_video/include/video_frame_buffer.h"
#include "rtc_base/checks.h"
#include "third_party/libyuv/include/libyuv.h"

namespace webrtc {

size_t CalcBufferSize(VideoType type, int width, int height) {
  RTC_DCHECK_GE(width, 0);
  RTC_DCHECK_GE(height, 0);
  size_t buffer_size = 0;
  switch (type) {
    case VideoType::kI420:
    case VideoType::kIYUV:
    case VideoType::kYV12: {
      int half_width = (width + 1) >> 1;
      int half_height = (height + 1) >> 1;
      buffer_size = width * height + half_width * half_height * 2;
      break;
    }
    case VideoType::kRGB565:
    case VideoType::kYUY2:
    case VideoType::kUYVY:
      buffer_size = width * height * 2;
      break;
    case VideoType::kRGB24:
      buffer_size = width * height * 3;
      break;
    case VideoType::kBGRA:
    case VideoType::kARGB:
      buffer_size = width * height * 4;
      break;
    default:
      RTC_NOTREACHED();
      break;
  }
  return buffer_size;
}

int ExtractBuffer(const rtc::scoped_refptr<I420BufferInterface>& input_frame,
                  size_t size,
                  uint8_t* buffer) {
  RTC_DCHECK(buffer);
  if (!input_frame)
    return -1;
  int width = input_frame->width();
  int height = input_frame->height();
  size_t length = CalcBufferSize(VideoType::kI420, width, height);
  if (size < length) {
    return -1;
  }

  int chroma_width = input_frame->ChromaWidth();
  int chroma_height = input_frame->ChromaHeight();

  libyuv::I420Copy(input_frame->DataY(), input_frame->StrideY(),
                   input_frame->DataU(), input_frame->StrideU(),
                   input_frame->DataV(), input_frame->StrideV(), buffer, width,
                   buffer + width * height, chroma_width,
                   buffer + width * height + chroma_width * chroma_height,
                   chroma_width, width, height);

  return static_cast<int>(length);
}

int ExtractBuffer(const VideoFrame& input_frame, size_t size, uint8_t* buffer) {
  return ExtractBuffer(input_frame.video_frame_buffer()->ToI420(), size,
                       buffer);
}

int ConvertVideoType(VideoType video_type) {
  switch (video_type) {
    case VideoType::kUnknown:
      return libyuv::FOURCC_ANY;
    case VideoType::kI420:
      return libyuv::FOURCC_I420;
    case VideoType::kIYUV:  // same as VideoType::kYV12
    case VideoType::kYV12:
      return libyuv::FOURCC_YV12;
    case VideoType::kRGB24:
      return libyuv::FOURCC_24BG;
    case VideoType::kRGB565:
      return libyuv::FOURCC_RGBP;
    case VideoType::kYUY2:
      return libyuv::FOURCC_YUY2;
    case VideoType::kUYVY:
      return libyuv::FOURCC_UYVY;
    case VideoType::kMJPEG:
      return libyuv::FOURCC_MJPG;
    case VideoType::kARGB:
      return libyuv::FOURCC_ARGB;
    case VideoType::kBGRA:
      return libyuv::FOURCC_BGRA;
  }
  RTC_NOTREACHED();
  return libyuv::FOURCC_ANY;
}

int ConvertFromI420(const VideoFrame& src_frame,
                    VideoType dst_video_type,
                    int dst_sample_size,
                    uint8_t* dst_frame) {
  rtc::scoped_refptr<I420BufferInterface> i420_buffer =
      src_frame.video_frame_buffer()->ToI420();
  return libyuv::ConvertFromI420(
      i420_buffer->DataY(), i420_buffer->StrideY(), i420_buffer->DataU(),
      i420_buffer->StrideU(), i420_buffer->DataV(), i420_buffer->StrideV(),
      dst_frame, dst_sample_size, src_frame.width(), src_frame.height(),
      ConvertVideoType(dst_video_type));
}

rtc::scoped_refptr<I420ABufferInterface> ScaleI420ABuffer(
    const I420ABufferInterface& buffer,
    int target_width,
    int target_height) {
  rtc::scoped_refptr<I420Buffer> yuv_buffer =
      I420Buffer::Create(target_width, target_height);
  yuv_buffer->ScaleFrom(buffer);
  rtc::scoped_refptr<I420Buffer> axx_buffer =
      I420Buffer::Create(target_width, target_height);
  libyuv::ScalePlane(buffer.DataA(), buffer.StrideA(), buffer.width(),
                     buffer.height(), axx_buffer->MutableDataY(),
                     axx_buffer->StrideY(), target_width, target_height,
                     libyuv::kFilterBox);
  rtc::scoped_refptr<I420ABufferInterface> merged_buffer = WrapI420ABuffer(
      yuv_buffer->width(), yuv_buffer->height(), yuv_buffer->DataY(),
      yuv_buffer->StrideY(), yuv_buffer->DataU(), yuv_buffer->StrideU(),
      yuv_buffer->DataV(), yuv_buffer->StrideV(), axx_buffer->DataY(),
      axx_buffer->StrideY(),
      // To keep references alive.
      [yuv_buffer, axx_buffer] {});
  return merged_buffer;
}

rtc::scoped_refptr<I420BufferInterface> ScaleVideoFrameBuffer(
    const I420BufferInterface& source,
    int dst_width,
    int dst_height) {
  rtc::scoped_refptr<I420Buffer> scaled_buffer =
      I420Buffer::Create(dst_width, dst_height);
  scaled_buffer->ScaleFrom(source);
  return scaled_buffer;
}

double I420SSE(const I420BufferInterface& ref_buffer,
               const I420BufferInterface& test_buffer) {
  RTC_DCHECK_EQ(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_EQ(ref_buffer.height(), test_buffer.height());
  const uint64_t width = test_buffer.width();
  const uint64_t height = test_buffer.height();
  const uint64_t sse_y = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataY(), ref_buffer.StrideY(), test_buffer.DataY(),
      test_buffer.StrideY(), width, height);
  const int width_uv = (width + 1) >> 1;
  const int height_uv = (height + 1) >> 1;
  const uint64_t sse_u = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataU(), ref_buffer.StrideU(), test_buffer.DataU(),
      test_buffer.StrideU(), width_uv, height_uv);
  const uint64_t sse_v = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataV(), ref_buffer.StrideV(), test_buffer.DataV(),
      test_buffer.StrideV(), width_uv, height_uv);
  const double samples = width * height + 2 * (width_uv * height_uv);
  const double sse = sse_y + sse_u + sse_v;
  return sse / (samples * 255.0 * 255.0);
}

// Compute PSNR for an I420A frame (all planes). Can upscale test frame.
double I420APSNR(const I420ABufferInterface& ref_buffer,
                 const I420ABufferInterface& test_buffer) {
  RTC_DCHECK_GE(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_GE(ref_buffer.height(), test_buffer.height());
  if ((ref_buffer.width() != test_buffer.width()) ||
      (ref_buffer.height() != test_buffer.height())) {
    rtc::scoped_refptr<I420ABufferInterface> scaled_buffer =
        ScaleI420ABuffer(test_buffer, ref_buffer.width(), ref_buffer.height());
    return I420APSNR(ref_buffer, *scaled_buffer);
  }
  const int width = test_buffer.width();
  const int height = test_buffer.height();
  const uint64_t sse_y = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataY(), ref_buffer.StrideY(), test_buffer.DataY(),
      test_buffer.StrideY(), width, height);
  const int width_uv = (width + 1) >> 1;
  const int height_uv = (height + 1) >> 1;
  const uint64_t sse_u = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataU(), ref_buffer.StrideU(), test_buffer.DataU(),
      test_buffer.StrideU(), width_uv, height_uv);
  const uint64_t sse_v = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataV(), ref_buffer.StrideV(), test_buffer.DataV(),
      test_buffer.StrideV(), width_uv, height_uv);
  const uint64_t sse_a = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataA(), ref_buffer.StrideA(), test_buffer.DataA(),
      test_buffer.StrideA(), width, height);
  const uint64_t samples = 2 * (uint64_t)width * (uint64_t)height +
                           2 * ((uint64_t)width_uv * (uint64_t)height_uv);
  const uint64_t sse = sse_y + sse_u + sse_v + sse_a;
  const double psnr = libyuv::SumSquareErrorToPsnr(sse, samples);
  return (psnr > kPerfectPSNR) ? kPerfectPSNR : psnr;
}

// Compute PSNR for an I420A frame (all planes)
double I420APSNR(const VideoFrame* ref_frame, const VideoFrame* test_frame) {
  if (!ref_frame || !test_frame)
    return -1;
  RTC_DCHECK(ref_frame->video_frame_buffer()->type() ==
             VideoFrameBuffer::Type::kI420A);
  RTC_DCHECK(test_frame->video_frame_buffer()->type() ==
             VideoFrameBuffer::Type::kI420A);
  return I420APSNR(*ref_frame->video_frame_buffer()->GetI420A(),
                   *test_frame->video_frame_buffer()->GetI420A());
}

// Compute PSNR for an I420 frame (all planes). Can upscale test frame.
double I420PSNR(const I420BufferInterface& ref_buffer,
                const I420BufferInterface& test_buffer) {
  RTC_DCHECK_GE(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_GE(ref_buffer.height(), test_buffer.height());
  if ((ref_buffer.width() != test_buffer.width()) ||
      (ref_buffer.height() != test_buffer.height())) {
    rtc::scoped_refptr<I420Buffer> scaled_buffer =
        I420Buffer::Create(ref_buffer.width(), ref_buffer.height());
    scaled_buffer->ScaleFrom(test_buffer);
    return I420PSNR(ref_buffer, *scaled_buffer);
  }
  double psnr = libyuv::I420Psnr(
      ref_buffer.DataY(), ref_buffer.StrideY(), ref_buffer.DataU(),
      ref_buffer.StrideU(), ref_buffer.DataV(), ref_buffer.StrideV(),
      test_buffer.DataY(), test_buffer.StrideY(), test_buffer.DataU(),
      test_buffer.StrideU(), test_buffer.DataV(), test_buffer.StrideV(),
      test_buffer.width(), test_buffer.height());
  // LibYuv sets the max psnr value to 128, we restrict it here.
  // In case of 0 mse in one frame, 128 can skew the results significantly.
  return (psnr > kPerfectPSNR) ? kPerfectPSNR : psnr;
}

// Compute PSNR for an I420 frame (all planes)
double I420PSNR(const VideoFrame* ref_frame, const VideoFrame* test_frame) {
  if (!ref_frame || !test_frame)
    return -1;
  return I420PSNR(*ref_frame->video_frame_buffer()->ToI420(),
                  *test_frame->video_frame_buffer()->ToI420());
}

// Compute SSIM for an I420A frame (all planes). Can upscale test frame.
double I420ASSIM(const I420ABufferInterface& ref_buffer,
                 const I420ABufferInterface& test_buffer) {
  RTC_DCHECK_GE(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_GE(ref_buffer.height(), test_buffer.height());
  if ((ref_buffer.width() != test_buffer.width()) ||
      (ref_buffer.height() != test_buffer.height())) {
    rtc::scoped_refptr<I420ABufferInterface> scaled_buffer =
        ScaleI420ABuffer(test_buffer, ref_buffer.width(), ref_buffer.height());
    return I420ASSIM(ref_buffer, *scaled_buffer);
  }
  const double yuv_ssim = libyuv::I420Ssim(
      ref_buffer.DataY(), ref_buffer.StrideY(), ref_buffer.DataU(),
      ref_buffer.StrideU(), ref_buffer.DataV(), ref_buffer.StrideV(),
      test_buffer.DataY(), test_buffer.StrideY(), test_buffer.DataU(),
      test_buffer.StrideU(), test_buffer.DataV(), test_buffer.StrideV(),
      test_buffer.width(), test_buffer.height());
  const double a_ssim = libyuv::CalcFrameSsim(
      ref_buffer.DataA(), ref_buffer.StrideA(), test_buffer.DataA(),
      test_buffer.StrideA(), test_buffer.width(), test_buffer.height());
  return (yuv_ssim + (a_ssim * 0.8)) / 1.8;
}

// Compute SSIM for an I420A frame (all planes)
double I420ASSIM(const VideoFrame* ref_frame, const VideoFrame* test_frame) {
  if (!ref_frame || !test_frame)
    return -1;
  RTC_DCHECK(ref_frame->video_frame_buffer()->type() ==
             VideoFrameBuffer::Type::kI420A);
  RTC_DCHECK(test_frame->video_frame_buffer()->type() ==
             VideoFrameBuffer::Type::kI420A);
  return I420ASSIM(*ref_frame->video_frame_buffer()->GetI420A(),
                   *test_frame->video_frame_buffer()->GetI420A());
}

// Compute SSIM for an I420 frame (all planes). Can upscale test_buffer.
double I420SSIM(const I420BufferInterface& ref_buffer,
                const I420BufferInterface& test_buffer) {
  RTC_DCHECK_GE(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_GE(ref_buffer.height(), test_buffer.height());
  if ((ref_buffer.width() != test_buffer.width()) ||
      (ref_buffer.height() != test_buffer.height())) {
    rtc::scoped_refptr<I420Buffer> scaled_buffer =
        I420Buffer::Create(ref_buffer.width(), ref_buffer.height());
    scaled_buffer->ScaleFrom(test_buffer);
    return I420SSIM(ref_buffer, *scaled_buffer);
  }
  return libyuv::I420Ssim(
      ref_buffer.DataY(), ref_buffer.StrideY(), ref_buffer.DataU(),
      ref_buffer.StrideU(), ref_buffer.DataV(), ref_buffer.StrideV(),
      test_buffer.DataY(), test_buffer.StrideY(), test_buffer.DataU(),
      test_buffer.StrideU(), test_buffer.DataV(), test_buffer.StrideV(),
      test_buffer.width(), test_buffer.height());
}

double I420SSIM(const VideoFrame* ref_frame, const VideoFrame* test_frame) {
  if (!ref_frame || !test_frame)
    return -1;
  return I420SSIM(*ref_frame->video_frame_buffer()->ToI420(),
                  *test_frame->video_frame_buffer()->ToI420());
}

}  // namespace webrtc
