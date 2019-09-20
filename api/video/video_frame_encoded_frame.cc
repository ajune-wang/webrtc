/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/video/video_frame.h"

#include <memory>

#include "api/video/encoded_frame.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

void VideoFrame::set_encoded_frame_source(
    std::unique_ptr<video_coding::EncodedFrame> encoded_frame) {
  encoded_frame_source_ =
      new rtc::RefCountedObject<EncodedFrameHolder>(std::move(encoded_frame));
}

rtc::scoped_refptr<VideoFrame::EncodedFrameHolder>
VideoFrame::get_encoded_frame_source() const {
  return encoded_frame_source_;
}

VideoFrame::EncodedFrameHolder::EncodedFrameHolder(
    std::unique_ptr<video_coding::EncodedFrame> encoded_frame)
    : encoded_frame_(std::move(encoded_frame)) {}

const video_coding::EncodedFrame& VideoFrame::EncodedFrameHolder::get() const {
  RTC_DCHECK(encoded_frame_.get());
  return *encoded_frame_;
}

}  // namespace webrtc
