/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/mappable_native_buffer.h"

#include "absl/algorithm/container.h"
#include "api/video/i420_buffer.h"
#include "api/video/nv12_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_rotation.h"
#include "common_video/include/video_frame_buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

namespace {

// An NV12Buffer, except ToI420() is not supported.
class NV12BufferWithoutToI420Support : public NV12Buffer {
 public:
  NV12BufferWithoutToI420Support(int width, int height)
      : NV12Buffer(width, height) {}

  rtc::scoped_refptr<I420BufferInterface> ToI420() override {
    // For zero-copy paths, we should never have to convert an NV12 frame to
    // I420.
    RTC_NOTREACHED()
        << "Attempting to convert a mapped MappableNativeBuffer to I420.";
    return nullptr;
  }
};

}  // namespace

VideoFrame CreateMappableNativeFrame(int64_t ntp_time_ms,
                                     VideoFrameBuffer::Type mappable_type,
                                     int width,
                                     int height) {
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(
                             new rtc::RefCountedObject<MappableNativeBuffer>(
                                 mappable_type, width, height))
                         .set_timestamp_rtp(99)
                         .set_timestamp_ms(99)
                         .set_rotation(kVideoRotation_0)
                         .build();
  frame.set_ntp_time_ms(ntp_time_ms);
  return frame;
}

rtc::scoped_refptr<MappableNativeBuffer> GetMappableNativeBufferFromVideoFrame(
    const VideoFrame& frame) {
  return static_cast<MappableNativeBuffer*>(frame.video_frame_buffer().get());
}

MappableNativeBuffer::ScaledBuffer::ScaledBuffer(
    rtc::scoped_refptr<MappableNativeBuffer> parent,
    int width,
    int height)
    : parent_(std::move(parent)), width_(width), height_(height) {}

MappableNativeBuffer::ScaledBuffer::~ScaledBuffer() {}

rtc::scoped_refptr<VideoFrameBuffer>
MappableNativeBuffer::ScaledBuffer::CropAndScale(int offset_x,
                                                 int offset_y,
                                                 int crop_width,
                                                 int crop_height,
                                                 int scaled_width,
                                                 int scaled_height) {
  return rtc::scoped_refptr<VideoFrameBuffer>(
      new rtc::RefCountedObject<ScaledBuffer>(parent_, scaled_width,
                                              scaled_height));
}

rtc::scoped_refptr<I420BufferInterface>
MappableNativeBuffer::ScaledBuffer::ToI420() {
  return parent_->GetOrCreateMappedBuffer(width_, height_)->ToI420();
}

rtc::scoped_refptr<VideoFrameBuffer>
MappableNativeBuffer::ScaledBuffer::GetMappedFrameBuffer(
    rtc::ArrayView<VideoFrameBuffer::Type> types) {
  if (absl::c_find(types, parent_->mappable_type_) == types.end())
    return nullptr;
  printf("GetMappedFrameBuffer correct type\n");
  return parent_->GetOrCreateMappedBuffer(width_, height_);
}

MappableNativeBuffer::MappableNativeBuffer(VideoFrameBuffer::Type mappable_type,
                                           int width,
                                           int height)
    : mappable_type_(mappable_type), width_(width), height_(height) {
  RTC_DCHECK(mappable_type == VideoFrameBuffer::Type::kI420 ||
             mappable_type == VideoFrameBuffer::Type::kNV12);
}

MappableNativeBuffer::~MappableNativeBuffer() {}

rtc::scoped_refptr<VideoFrameBuffer> MappableNativeBuffer::CropAndScale(
    int offset_x,
    int offset_y,
    int crop_width,
    int crop_height,
    int scaled_width,
    int scaled_height) {
  return FullSizeBuffer()->CropAndScale(
      offset_x, offset_y, crop_width, crop_height, scaled_width, scaled_height);
}

rtc::scoped_refptr<I420BufferInterface> MappableNativeBuffer::ToI420() {
  return FullSizeBuffer()->ToI420();
}

rtc::scoped_refptr<VideoFrameBuffer> MappableNativeBuffer::GetMappedFrameBuffer(
    rtc::ArrayView<VideoFrameBuffer::Type> types) {
  return FullSizeBuffer()->GetMappedFrameBuffer(types);
}

std::vector<rtc::scoped_refptr<VideoFrameBuffer>>
MappableNativeBuffer::GetMappedFramedBuffers() const {
  MutexLock lock(&lock_);
  return mapped_buffers_;
}

rtc::scoped_refptr<MappableNativeBuffer::ScaledBuffer>
MappableNativeBuffer::FullSizeBuffer() {
  return rtc::scoped_refptr<MappableNativeBuffer::ScaledBuffer>(
      new rtc::RefCountedObject<ScaledBuffer>(this, width_, height_));
}

rtc::scoped_refptr<VideoFrameBuffer>
MappableNativeBuffer::GetOrCreateMappedBuffer(int width, int height) {
  MutexLock lock(&lock_);
  for (auto& mapped_buffer : mapped_buffers_) {
    if (mapped_buffer->width() == width && mapped_buffer->height() == height) {
      return mapped_buffer;
    }
  }
  rtc::scoped_refptr<VideoFrameBuffer> mapped_buffer;
  switch (mappable_type_) {
    case VideoFrameBuffer::Type::kI420: {
      rtc::scoped_refptr<I420Buffer> i420_buffer =
          I420Buffer::Create(width, height);
      I420Buffer::SetBlack(i420_buffer);
      mapped_buffer = i420_buffer;
      break;
    }
    case VideoFrameBuffer::Type::kNV12: {
      rtc::scoped_refptr<NV12BufferWithoutToI420Support> nv12_buffer(
          new rtc::RefCountedObject<NV12BufferWithoutToI420Support>(width,
                                                                    height));
      nv12_buffer->InitializeData();
      mapped_buffer = nv12_buffer;
      break;
    }
    default:
      RTC_NOTREACHED();
  }
  mapped_buffers_.push_back(mapped_buffer);
  return mapped_buffer;
}

}  // namespace test
}  // namespace webrtc
