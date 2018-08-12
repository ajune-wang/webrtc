/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/multiplex/include/multiplex_video_frame_buffer.h"
#include "api/video/video_frame_buffer.h"

namespace webrtc {

MultiplexVideoFrameBuffer::MultiplexVideoFrameBuffer(
    const rtc::scoped_refptr<VideoFrameBuffer>& video_frame_buffer,
    uint8_t* augmenting_data,
    const uint16_t& augmenting_data_size)
    : augmenting_data_size_(augmenting_data_size),
      augmenting_data_(augmenting_data),
      video_frame_buffer_(video_frame_buffer) {}

rtc::scoped_refptr<VideoFrameBuffer>
MultiplexVideoFrameBuffer::GetVideoFrameBuffer() const {
  return video_frame_buffer_;
}

uint8_t* MultiplexVideoFrameBuffer::GetAndReleaseAugmentingData() {
  return augmenting_data_.release();
}

uint16_t MultiplexVideoFrameBuffer::GetAugmentingDataSize() const {
  return augmenting_data_size_;
}

VideoFrameBuffer::Type MultiplexVideoFrameBuffer::type() const {
  return video_frame_buffer_->type();
}

int MultiplexVideoFrameBuffer::width() const {
  return video_frame_buffer_->width();
}

int MultiplexVideoFrameBuffer::height() const {
  return video_frame_buffer_->height();
}

rtc::scoped_refptr<I420BufferInterface> MultiplexVideoFrameBuffer::ToI420() {
  return video_frame_buffer_->ToI420();
}
}  // namespace webrtc
