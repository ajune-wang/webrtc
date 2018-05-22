/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/videotracksource.h"

#include <utility>

namespace webrtc {

VideoTrackSource::VideoTrackSource(
    std::unique_ptr<rtc::VideoSourceInterface<VideoFrame>> source,
    bool remote)
    : source_(std::move(source)), state_(kInitializing), remote_(remote) {
  worker_thread_checker_.DetachFromThread();
}

void VideoTrackSource::SetState(SourceState new_state) {
  if (state_ != new_state) {
    state_ = new_state;
    FireOnChanged();
  }
}

void VideoTrackSource::AddOrUpdateSink(
    rtc::VideoSinkInterface<VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  source_->AddOrUpdateSink(sink, wants);
}

void VideoTrackSource::RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  source_->RemoveSink(sink);
}

}  //  namespace webrtc
