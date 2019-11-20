/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/video_rtp_track_source.h"

namespace webrtc {

VideoRtpTrackSource::VideoRtpTrackSource(Callback* callback)
    : VideoTrackSource(true /* remote */), callback_(callback) {}

void VideoRtpTrackSource::ClearCallback() {
  rtc::CritScope cs(&mu_);
  callback_ = nullptr;
}

rtc::VideoSourceInterface<VideoFrame>* VideoRtpTrackSource::source() {
  return &broadcaster_;
}
rtc::VideoSinkInterface<VideoFrame>* VideoRtpTrackSource::sink() {
  return &broadcaster_;
}

void VideoRtpTrackSource::BroadcastEncodedFrameBuffer(
    rtc::scoped_refptr<VideoEncodedSinkInterface::FrameBuffer> frame_buffer) {
  rtc::CritScope cs(&mu_);
  for (VideoEncodedSinkInterface* sink : encoded_sinks_) {
    sink->OnEncodedFrame(frame_buffer);
  }
}

bool VideoRtpTrackSource::SupportsEncodedOutput() const {
  return true;
}

void VideoRtpTrackSource::GenerateKeyFrame() {
  rtc::CritScope cs(&mu_);
  if (callback_) {
    callback_->OnGenerateKeyFrame();
  }
}

void VideoRtpTrackSource::AddEncodedSink(VideoEncodedSinkInterface* sink) {
  RTC_DCHECK(sink);
  {
    rtc::CritScope cs(&mu_);
    RTC_DCHECK(std::find(encoded_sinks_.begin(), encoded_sinks_.end(), sink) ==
               encoded_sinks_.end());
    encoded_sinks_.push_back(sink);
    if (encoded_sinks_.size() == 1 && callback_) {
      callback_->OnEncodedSinkEnabled(true);
    }
  }
}

void VideoRtpTrackSource::RemoveEncodedSink(VideoEncodedSinkInterface* sink) {
  rtc::CritScope cs(&mu_);
  auto it = std::find(encoded_sinks_.begin(), encoded_sinks_.end(), sink);
  if (it != encoded_sinks_.end()) {
    encoded_sinks_.erase(it);
  }
  if (encoded_sinks_.size() == 0 && callback_) {
    callback_->OnEncodedSinkEnabled(false);
  }
}

}  // namespace webrtc
