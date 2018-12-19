/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/test_video_capturer.h"

#include <algorithm>

#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {
namespace test {
TestVideoCapturer::TestVideoCapturer()
    : video_adapter_(new cricket::VideoAdapter()) {}
TestVideoCapturer::~TestVideoCapturer() {}

void TestVideoCapturer::AdaptFrame(const VideoFrame& frame) {
  int cropped_width = 0;
  int cropped_height = 0;
  int out_width = 0;
  int out_height = 0;

  if (!video_adapter_->AdaptFrameResolution(
          frame.width(), frame.height(), frame.timestamp_us() * 1000,
          &cropped_width, &cropped_height, &out_width, &out_height)) {
    // Drop frame in order to respect frame rate constraint.
    return;
  }

  if (out_height != frame.height() || out_width != frame.width()) {
    // Video adapter has requested a down-scale. Allocate a new buffer and
    // return scaled version.
    rtc::scoped_refptr<I420Buffer> scaled_buffer =
        I420Buffer::Create(out_width, out_height);
    scaled_buffer->ScaleFrom(*frame.video_frame_buffer()->ToI420());
    OnFrame(VideoFrame(scaled_buffer, kVideoRotation_0, frame.timestamp_us()));
  } else {
    // No adaptations needed, just return the frame as is.
    OnFrame(frame);
  }
}

void TestVideoCapturer::OnFrame(const VideoFrame& frame) {
  rtc::CritScope cs(&sink_lock_);
  for (const auto& sink : sinks_) {
    sink.sink->OnFrame(frame);
  }
}
rtc::VideoSinkWants TestVideoCapturer::GetSinkWants() {
  rtc::CritScope cs(&sink_lock_);
  return current_wants_;
}

void TestVideoCapturer::AddOrUpdateSink(
    rtc::VideoSinkInterface<VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  rtc::CritScope cs(&sink_lock_);
  auto sink_pair_it = std::find_if(
      sinks_.begin(), sinks_.end(),
      [sink](const SinkPair& sink_pair) { return sink_pair.sink == sink; });

  if (sink_pair_it == sinks_.end()) {
    sinks_.emplace_back(sink, wants);
  } else {
    sink_pair_it->wants = wants;
  }
  UpdateSinkWants();
}

void TestVideoCapturer::RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) {
  rtc::CritScope cs(&sink_lock_);
  sinks_.erase(std::remove_if(sinks_.begin(), sinks_.end(),
                              [sink](const SinkPair& sink_pair) {
                                return sink_pair.sink == sink;
                              }),
               sinks_.end());
  UpdateSinkWants();
}

void TestVideoCapturer::UpdateSinkWants() {
  // Duplicates logic in VideoBroadcaster::UpdateWants.
  rtc::VideoSinkWants wants;
  wants.rotation_applied = false;
  for (auto& sink : sinks_) {
    // wants.rotation_applied == ANY(sink.wants.rotation_applied)
    if (sink.wants.rotation_applied) {
      wants.rotation_applied = true;
    }
    // wants.max_pixel_count == MIN(sink.wants.max_pixel_count)
    if (sink.wants.max_pixel_count < wants.max_pixel_count) {
      wants.max_pixel_count = sink.wants.max_pixel_count;
    }
    // Select the minimum requested target_pixel_count, if any, of all sinks so
    // that we don't over utilize the resources for any one.
    // TODO(sprang): Consider using the median instead, since the limit can be
    // expressed by max_pixel_count.
    if (sink.wants.target_pixel_count &&
        (!wants.target_pixel_count ||
         (*sink.wants.target_pixel_count < *wants.target_pixel_count))) {
      wants.target_pixel_count = sink.wants.target_pixel_count;
    }
    // Select the minimum for the requested max framerates.
    if (sink.wants.max_framerate_fps < wants.max_framerate_fps) {
      wants.max_framerate_fps = sink.wants.max_framerate_fps;
    }
  }

  if (wants.target_pixel_count &&
      *wants.target_pixel_count >= wants.max_pixel_count) {
    wants.target_pixel_count.emplace(wants.max_pixel_count);
  }

  video_adapter_->OnResolutionFramerateRequest(
      wants.target_pixel_count, wants.max_pixel_count, wants.max_framerate_fps);

  current_wants_ = wants;
}

}  // namespace test
}  // namespace webrtc
