/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TEST_VIDEO_CAPTURER_H_
#define TEST_TEST_VIDEO_CAPTURER_H_

#include <stddef.h>

#include <memory>

#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "media/base/videoadapter.h"
#include "rtc_base/criticalsection.h"

namespace cricket {
class VideoAdapter;
}  // namespace cricket

namespace webrtc {
class Clock;
namespace test {

class TestVideoCapturer : public rtc::VideoSourceInterface<VideoFrame> {
 public:
  TestVideoCapturer();
  virtual ~TestVideoCapturer();

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override;

 protected:
  void AdaptFrame(const VideoFrame& frame);
  rtc::VideoSinkWants GetSinkWants();

 private:
  struct SinkPair {
    SinkPair(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
             rtc::VideoSinkWants wants)
        : sink(sink), wants(wants) {}
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink;
    rtc::VideoSinkWants wants;
  };

  void OnFrame(const VideoFrame& frame);

  void UpdateSinkWants() RTC_EXCLUSIVE_LOCKS_REQUIRED(sink_lock_);

  const std::unique_ptr<cricket::VideoAdapter> video_adapter_;
  rtc::CriticalSection sink_lock_;
  std::vector<SinkPair> sinks_ RTC_GUARDED_BY(sink_lock_);
  rtc::VideoSinkWants current_wants_ RTC_GUARDED_BY(sink_lock_);
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_TEST_VIDEO_CAPTURER_H_
