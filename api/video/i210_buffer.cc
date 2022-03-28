/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/video/I210_buffer.h"

#include <utility>

#include "api/video/i010_buffer.h"
#include "api/video/i420_buffer.h"
#include "api/video/i422_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_counted_object.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/scale.h"

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;
static const int kBytesPerPixel = 2;

namespace webrtc {

namespace {

int I210DataSize(int height, int stride_y, int stride_u, int stride_v) {
  return kBytesPerPixel *
         (stride_y * height + stride_u * height + stride_v * height);
}

int I210Copy(const uint16_t* src_y,
             int src_stride_y,
             const uint16_t* src_u,
             int src_stride_u,
             const uint16_t* src_v,
             int src_stride_v,
             uint16_t* dst_y,
             int dst_stride_y,
             uint16_t* dst_u,
             int dst_stride_u,
             uint16_t* dst_v,
             int dst_stride_v,
             int width,
             int height) {
  int halfwidth = (width + 1) >> 1;
  if (!src_u || !src_v || !dst_u || !dst_v || width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_y = src_y + (height - 1) * src_stride_y;
    src_u = src_u + (height - 1) * src_stride_u;
    src_v = src_v + (height - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

  if (dst_y) {
    libyuv::CopyPlane_16(src_y, src_stride_y, dst_y, dst_stride_y, width,
                         height);
  }
  // Copy UV planes.
  libyuv::CopyPlane_16(src_u, src_stride_u, dst_u, dst_stride_u, halfwidth,
                       height);
  libyuv::CopyPlane_16(src_v, src_stride_v, dst_v, dst_stride_v, halfwidth,
                       height);
  return 0;
}

int I422Scale_16(const uint16_t* src_y,
                 int src_stride_y,
                 const uint16_t* src_u,
                 int src_stride_u,
                 const uint16_t* src_v,
                 int src_stride_v,
                 int src_width,
                 int src_height,
                 uint16_t* dst_y,
                 int dst_stride_y,
                 uint16_t* dst_u,
                 int dst_stride_u,
                 uint16_t* dst_v,
                 int dst_stride_v,
                 int dst_width,
                 int dst_height,
                 enum libyuv::FilterMode filtering) {
  if (!src_y || !src_u || !src_v || src_width <= 0 || src_height == 0 ||
      src_width > 32768 || src_height > 32768 || !dst_y || !dst_u || !dst_v ||
      dst_width <= 0 || dst_height <= 0) {
    return -1;
  }

  libyuv::ScalePlane_16(src_y, src_stride_y, src_width, src_height, dst_y,
                        dst_stride_y, dst_width, dst_height, filtering);
  libyuv::ScalePlane_16(src_u, src_stride_u, src_width, src_height, dst_u,
                        dst_stride_u, dst_width, dst_height, filtering);
  libyuv::ScalePlane_16(src_v, src_stride_v, src_width, src_height, dst_v,
                        dst_stride_v, dst_width, dst_height, filtering);
  return 0;
}

int I422ToI210(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint16_t* dst_y,
               int dst_stride_y,
               uint16_t* dst_u,
               int dst_stride_u,
               uint16_t* dst_v,
               int dst_stride_v,
               int width,
               int height) {
  int halfwidth = (width + 1) >> 1;
  if (!src_u || !src_v || !dst_u || !dst_v || width <= 0 || height == 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_y = src_y + (height - 1) * src_stride_y;
    src_u = src_u + (height - 1) * src_stride_u;
    src_v = src_v + (height - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

  // Convert Y plane.
  libyuv::Convert8To16Plane(src_y, src_stride_y, dst_y, dst_stride_y, 1024,
                            width, height);
  // Convert UV planes.
  libyuv::Convert8To16Plane(src_u, src_stride_u, dst_u, dst_stride_u, 1024,
                            halfwidth, height);
  libyuv::Convert8To16Plane(src_v, src_stride_v, dst_v, dst_stride_v, 1024,
                            halfwidth, height);
  return 0;
}

}  // namespace

I210Buffer::I210Buffer(int width,
                       int height,
                       int stride_y,
                       int stride_u,
                       int stride_v)
    : width_(width),
      height_(height),
      stride_y_(stride_y),
      stride_u_(stride_u),
      stride_v_(stride_v),
      data_(static_cast<uint16_t*>(
          AlignedMalloc(I210DataSize(height, stride_y, stride_u, stride_v),
                        kBufferAlignment))) {
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
  RTC_DCHECK_GE(stride_y, width);
  RTC_DCHECK_GE(stride_u, (width + 1) / 2);
  RTC_DCHECK_GE(stride_v, (width + 1) / 2);
}

I210Buffer::~I210Buffer() {}

// static
rtc::scoped_refptr<I210Buffer> I210Buffer::Create(int width, int height) {
  return rtc::make_ref_counted<I210Buffer>(width, height, width,
                                           (width + 1) / 2, (width + 1) / 2);
}

// static
rtc::scoped_refptr<I210Buffer> I210Buffer::Copy(
    const I210BufferInterface& source) {
  const int width = source.width();
  const int height = source.height();
  rtc::scoped_refptr<I210Buffer> buffer = Create(width, height);
  RTC_CHECK_EQ(
      0, I210Copy(source.DataY(), source.StrideY(), source.DataU(),
                  source.StrideU(), source.DataV(), source.StrideV(),
                  buffer->MutableDataY(), buffer->StrideY(),
                  buffer->MutableDataU(), buffer->StrideU(),
                  buffer->MutableDataV(), buffer->StrideV(), width, height));
  return buffer;
}

// static
rtc::scoped_refptr<I210Buffer> I210Buffer::Copy(
    const I420BufferInterface& source) {
  const int width = source.width();
  const int height = source.height();
  auto i422buffer = I422Buffer::Copy(source);
  rtc::scoped_refptr<I210Buffer> buffer = Create(width, height);
  RTC_CHECK_EQ(
      0, I422ToI210(i422buffer->DataY(), i422buffer->StrideY(),
                    i422buffer->DataU(), i422buffer->StrideU(),
                    i422buffer->DataV(), i422buffer->StrideV(),
                    buffer->MutableDataY(), buffer->StrideY(),
                    buffer->MutableDataU(), buffer->StrideU(),
                    buffer->MutableDataV(), buffer->StrideV(), width, height));
  return buffer;
}

// static
rtc::scoped_refptr<I210Buffer> I210Buffer::Rotate(
    const I210BufferInterface& src,
    VideoRotation rotation) {
  if (rotation == webrtc::kVideoRotation_0)
    return Copy(src);

  RTC_CHECK(src.DataY());
  RTC_CHECK(src.DataU());
  RTC_CHECK(src.DataV());
  int rotated_width = src.width();
  int rotated_height = src.height();
  if (rotation == webrtc::kVideoRotation_90 ||
      rotation == webrtc::kVideoRotation_270) {
    std::swap(rotated_width, rotated_height);
  }

  rtc::scoped_refptr<webrtc::I210Buffer> buffer =
      Create(rotated_width, rotated_height);
  // Remove this when there is libyuv::I210Rotate().
  for (int x = 0; x < src.width(); x++) {
    for (int y = 0; y < src.height(); y++) {
      int dest_x = x;
      int dest_y = y;
      switch (rotation) {
        // This case is covered by the early return.
        case webrtc::kVideoRotation_0:
          RTC_DCHECK_NOTREACHED();
          break;
        case webrtc::kVideoRotation_90:
          dest_x = src.height() - y - 1;
          dest_y = x;
          break;
        case webrtc::kVideoRotation_180:
          dest_x = src.width() - x - 1;
          dest_y = src.height() - y - 1;
          break;
        case webrtc::kVideoRotation_270:
          dest_x = y;
          dest_y = src.width() - x - 1;
          break;
      }
      buffer->MutableDataY()[dest_x + buffer->StrideY() * dest_y] =
          src.DataY()[x + src.StrideY() * y];
      dest_x /= 2;
      int src_x = x / 2;
      int src_y = y;
      buffer->MutableDataU()[dest_x + buffer->StrideU() * dest_y] =
          src.DataU()[src_x + src.StrideU() * src_y];
      buffer->MutableDataV()[dest_x + buffer->StrideV() * dest_y] =
          src.DataV()[src_x + src.StrideV() * src_y];
    }
  }
  return buffer;
}

rtc::scoped_refptr<I420BufferInterface> I210Buffer::ToI420() {
  rtc::scoped_refptr<I420Buffer> i420_buffer =
      I420Buffer::Create(width(), height());
  auto i010buffer = I010Buffer::Create(width(), height());
  libyuv::I210ToI010(DataY(), StrideY(), DataU(), StrideU(), DataV(), StrideV(),
                     i010buffer->MutableDataY(), i010buffer->StrideY(),
                     i010buffer->MutableDataU(), i010buffer->StrideU(),
                     i010buffer->MutableDataV(), i010buffer->StrideV(), width(),
                     height());
  return i010buffer->ToI420();
}

int I210Buffer::width() const {
  return width_;
}

int I210Buffer::height() const {
  return height_;
}

const uint16_t* I210Buffer::DataY() const {
  return data_.get();
}
const uint16_t* I210Buffer::DataU() const {
  return data_.get() + stride_y_ * height_;
}
const uint16_t* I210Buffer::DataV() const {
  return data_.get() + stride_y_ * height_ + stride_u_ * height_;
}

int I210Buffer::StrideY() const {
  return stride_y_;
}
int I210Buffer::StrideU() const {
  return stride_u_;
}
int I210Buffer::StrideV() const {
  return stride_v_;
}

uint16_t* I210Buffer::MutableDataY() {
  return const_cast<uint16_t*>(DataY());
}
uint16_t* I210Buffer::MutableDataU() {
  return const_cast<uint16_t*>(DataU());
}
uint16_t* I210Buffer::MutableDataV() {
  return const_cast<uint16_t*>(DataV());
}

void I210Buffer::CropAndScaleFrom(const I210BufferInterface& src,
                                  int offset_x,
                                  int offset_y,
                                  int crop_width,
                                  int crop_height) {
  RTC_CHECK_LE(crop_width, src.width());
  RTC_CHECK_LE(crop_height, src.height());
  RTC_CHECK_LE(crop_width + offset_x, src.width());
  RTC_CHECK_LE(crop_height + offset_y, src.height());
  RTC_CHECK_GE(offset_x, 0);
  RTC_CHECK_GE(offset_y, 0);

  // Make sure offset is even so that u/v plane becomes aligned.
  const int uv_offset_x = offset_x / 2;
  const int uv_offset_y = offset_y / 2;
  offset_x = uv_offset_x * 2;
  offset_y = uv_offset_y * 2;

  const uint16_t* y_plane = src.DataY() + src.StrideY() * offset_y + offset_x;
  const uint16_t* u_plane =
      src.DataU() + src.StrideU() * uv_offset_y + uv_offset_x;
  const uint16_t* v_plane =
      src.DataV() + src.StrideV() * uv_offset_y + uv_offset_x;
  int res =
      I422Scale_16(y_plane, src.StrideY(), u_plane, src.StrideU(), v_plane,
                   src.StrideV(), crop_width, crop_height, MutableDataY(),
                   StrideY(), MutableDataU(), StrideU(), MutableDataV(),
                   StrideV(), width(), height(), libyuv::kFilterBox);

  RTC_DCHECK_EQ(res, 0);
}

void I210Buffer::ScaleFrom(const I210BufferInterface& src) {
  CropAndScaleFrom(src, 0, 0, src.width(), src.height());
}

}  // namespace webrtc
