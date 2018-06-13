/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_FAKEVIDEOSOURCE_H_
#define MEDIA_BASE_FAKEVIDEOSOURCE_H_

#include "api/videosourceinterface.h"

namespace webrtc {

class FakeVideoSource : public rtc::VideoSourceInterface<VideoFrame> {
 public:
  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {
    ++count_;
    latest_wants_ = wants;
  }

  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override {
    RTC_CHECK_GT(count_, 0);
  }

  bool apply_rotation() {
    RTC_CHECK(count_ > 0);
    return latest_wants_.rotation_applied;
  }

 private:
  int count_ = 0;
  rtc::VideoSinkWants latest_wants_;
};

}  // namespace webrtc

#endif  // MEDIA_BASE_FAKEVIDEOSOURCE_H_

#include "api/video/video_frame.h"
