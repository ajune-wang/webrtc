/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_ENCODED_IMAGE_H_
#define API_VIDEO_ENCODED_IMAGE_H_

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/video/color_space.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_rotation.h"
#include "api/video/video_timing.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// TODO(bug.webrtc.org/9378): This is a legacy api class, which is slowly being
// cleaned up. Direct use of its members is strongly discouraged.
class RTC_EXPORT EncodedImage {
 public:
  EncodedImage();
  EncodedImage(EncodedImage&&);
  EncodedImage(const EncodedImage&);

  ~EncodedImage();

  EncodedImage& operator=(EncodedImage&&);
  EncodedImage& operator=(const EncodedImage&);

  // TODO(nisse): Change style to timestamp(), set_timestamp(), for consistency
  // with the VideoFrame class.
  // Set frame timestamp (90kHz).
  void SetTimestamp(uint32_t timestamp) { timestamp_rtp_ = timestamp; }

  // Get frame timestamp (90kHz).
  uint32_t Timestamp() const { return timestamp_rtp_; }

  void SetEncodeTime(int64_t encode_start_ms, int64_t encode_finish_ms);

  absl::optional<int> SpatialIndex() const {
    return spatial_index_;
  }
  void SetSpatialIndex(absl::optional<int> spatial_index) {
    RTC_DCHECK_GE(spatial_index.value_or(0), 0);
    RTC_DCHECK_LT(spatial_index.value_or(0), kMaxSpatialLayers);
    spatial_index_ = spatial_index;
  }

  const webrtc::ColorSpace* ColorSpace() const {
    return color_space_ ? &*color_space_ : nullptr;
  }
  void SetColorSpace(const absl::optional<webrtc::ColorSpace>& color_space) {
    color_space_ = color_space;
  }

  size_t size() const { return size_; }
  void set_size(size_t new_size) {
    RTC_DCHECK_LE(new_size, capacity());
    size_ = new_size;
  }
  size_t capacity() const { return capacity_; }

  // The data is a segment of a larger buffer, should be considered immutable.
  void set_buffer(rtc::CopyOnWriteBuffer buffer,
                  size_t offset,
                  size_t capacity) {
    RTC_DCHECK_LE(offset + capacity, buffer.capacity());
    buffer_ = buffer;
    offset_ = offset;
    capacity_ = capacity;
    size_ = 0;
  }

  void Allocate(size_t capacity) {
    buffer_.SetSize(capacity);
    capacity_ = capacity;
    offset_ = 0;
  }

  uint8_t* data() { return buffer_.data() + offset_; }
  const uint8_t* data() const { return buffer_.cdata() + offset_; }

  rtc::CopyOnWriteBuffer buffer() const {
    // TODO(nisse): Will give unexpected results with offset_ > 0. To get that
    // case working correctly, we would need to either expose offset_, or make a
    // copy of the relevant portion of the buffer, or move the offset inside
    // rtc::CopyOnWriteBuffer.
    RTC_DCHECK_EQ(offset_, 0);
    return buffer_;
  }

  uint32_t _encodedWidth = 0;
  uint32_t _encodedHeight = 0;
  // NTP time of the capture time in local timebase in milliseconds.
  int64_t ntp_time_ms_ = 0;
  int64_t capture_time_ms_ = 0;
  VideoFrameType _frameType = VideoFrameType::kVideoFrameDelta;
  VideoRotation rotation_ = kVideoRotation_0;
  VideoContentType content_type_ = VideoContentType::UNSPECIFIED;
  bool _completeFrame = false;
  int qp_ = -1;  // Quantizer value.

  // When an application indicates non-zero values here, it is taken as an
  // indication that all future frames will be constrained with those limits
  // until the application indicates a change again.
  PlayoutDelay playout_delay_ = {-1, -1};

  struct Timing {
    uint8_t flags = VideoSendTiming::kInvalid;
    int64_t encode_start_ms = 0;
    int64_t encode_finish_ms = 0;
    int64_t packetization_finish_ms = 0;
    int64_t pacer_exit_ms = 0;
    int64_t network_timestamp_ms = 0;
    int64_t network2_timestamp_ms = 0;
    int64_t receive_start_ms = 0;
    int64_t receive_finish_ms = 0;
  } timing_;

 private:
  rtc::CopyOnWriteBuffer buffer_;
  // Offset to start of this frame's data.
  size_t offset_ = 0;
  size_t size_ = 0;  // Size of encoded frame data.
  // Allocated size.
  size_t capacity_ = 0;
  uint32_t timestamp_rtp_ = 0;
  absl::optional<int> spatial_index_;
  absl::optional<webrtc::ColorSpace> color_space_;
};

}  // namespace webrtc

#endif  // API_VIDEO_ENCODED_IMAGE_H_
