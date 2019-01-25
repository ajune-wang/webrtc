/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/frame_generator_capturer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

FrameGeneratorCapturer* FrameGeneratorCapturer::Create(
    int width,
    int height,
    absl::optional<FrameGenerator::OutputType> type,
    absl::optional<int> num_squares,
    int target_fps,
    Clock* clock) {
  return new FrameGeneratorCapturer(
      clock,
      FrameGenerator::CreateSquareGenerator(width, height, type, num_squares),
      target_fps);
}

FrameGeneratorCapturer* FrameGeneratorCapturer::CreateFromYuvFile(
    const std::string& file_name,
    size_t width,
    size_t height,
    int target_fps,
    Clock* clock) {
  return new FrameGeneratorCapturer(
      clock,
      FrameGenerator::CreateFromYuvFile(std::vector<std::string>(1, file_name),
                                        width, height, 1),
      target_fps);
}

FrameGeneratorCapturer* FrameGeneratorCapturer::CreateSlideGenerator(
    int width,
    int height,
    int frame_repeat_count,
    int target_fps,
    Clock* clock) {
  return new FrameGeneratorCapturer(
      clock,
      FrameGenerator::CreateSlideGenerator(width, height, frame_repeat_count),
      target_fps);
}

FrameGeneratorCapturer::FrameGeneratorCapturer(
    Clock* clock,
    std::unique_ptr<FrameGenerator> frame_generator,
    int target_fps)
    : clock_(clock),
      sink_wants_observer_(nullptr),
      frame_generator_(std::move(frame_generator)),
      source_fps_(target_fps),
      target_capture_fps_(target_fps),
      first_frame_capture_time_(-1),
      task_queue_("FrameGenCapQ", rtc::TaskQueue::Priority::HIGH) {
  RTC_DCHECK(frame_generator_);
  RTC_DCHECK_GT(target_fps, 0);
}

FrameGeneratorCapturer::~FrameGeneratorCapturer() {}

void FrameGeneratorCapturer::SetFakeRotation(VideoRotation rotation) {
  task_queue_.PostTask([this, rotation] {
    RTC_DCHECK_RUN_ON(&task_queue_);
    fake_rotation_ = rotation;
  });
}

void FrameGeneratorCapturer::SetFakeColorSpace(
    absl::optional<ColorSpace> color_space) {
  task_queue_.PostTask([this, color_space] {
    RTC_DCHECK_RUN_ON(&task_queue_);
    fake_color_space_ = color_space;
  });
}

void FrameGeneratorCapturer::InsertFrame() {
    VideoFrame* frame = frame_generator_->NextFrame();
    // TODO(srte): Use more advanced frame rate control to allow arbritrary
    // fractions.
    int decimation =
        std::round(static_cast<double>(source_fps_) / target_capture_fps_);
    for (int i = 1; i < decimation; ++i)
      frame = frame_generator_->NextFrame();
    frame->set_timestamp_us(clock_->TimeInMicroseconds());
    frame->set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());
    frame->set_rotation(fake_rotation_);
    if (fake_color_space_) {
      frame->set_color_space(&fake_color_space_.value());
    }
    if (first_frame_capture_time_ == -1) {
      first_frame_capture_time_ = frame->ntp_time_ms();
    }

    rtc::CriticalSection lock_;
    TestVideoCapturer::OnFrame(*frame);
}

void FrameGeneratorCapturer::Start() {
  if (insert_frame_task_.Running())
    return;
  insert_frame_task_ = RepeatingTaskHandle::Start(&task_queue_, [this] {
    RTC_DCHECK_RUN_ON(&task_queue_);
    InsertFrame();
    return GetCurrentFrameInterval();
  });
}

void FrameGeneratorCapturer::Stop() {
  insert_frame_task_.PostStop();
}

void FrameGeneratorCapturer::ChangeResolution(size_t width, size_t height) {
  task_queue_.PostTask([this, width, height] {
    frame_generator_->ChangeResolution(width, height);
  });
}

void FrameGeneratorCapturer::ChangeFramerate(int target_framerate) {
  task_queue_.PostTask([this, target_framerate] {
    RTC_DCHECK_RUN_ON(&task_queue_);
    RTC_CHECK(target_capture_fps_ > 0);
    if (target_framerate > source_fps_)
      RTC_LOG(LS_WARNING) << "Target framerate clamped from "
                          << target_framerate << " to " << source_fps_;
    if (source_fps_ % target_capture_fps_ != 0) {
      int decimation =
          std::round(static_cast<double>(source_fps_) / target_capture_fps_);
      int effective_rate = target_capture_fps_ / decimation;
      RTC_LOG(LS_WARNING) << "Target framerate, " << target_framerate
                          << ", is an uneven fraction of the source rate, "
                          << source_fps_
                          << ". The framerate will be :" << effective_rate;
    }
    target_capture_fps_ = std::min(source_fps_, target_framerate);
  });
}

void FrameGeneratorCapturer::SetSinkWantsObserver(SinkWantsObserver* observer) {
  rtc::CritScope cs(&lock_);
  RTC_DCHECK(!sink_wants_observer_);
  sink_wants_observer_ = observer;
}

void FrameGeneratorCapturer::AddOrUpdateSink(
    rtc::VideoSinkInterface<VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  TestVideoCapturer::AddOrUpdateSink(sink, wants);
  rtc::CritScope cs(&lock_);
  if (sink_wants_observer_) {
    // Tests need to observe unmodified sink wants.
    sink_wants_observer_->OnSinkWantsChanged(sink, wants);
  }
  UpdateFps(GetSinkWants().max_framerate_fps);
}

void FrameGeneratorCapturer::RemoveSink(
    rtc::VideoSinkInterface<VideoFrame>* sink) {
  TestVideoCapturer::RemoveSink(sink);

  rtc::CritScope cs(&lock_);
  UpdateFps(GetSinkWants().max_framerate_fps);
}

void FrameGeneratorCapturer::UpdateFps(int max_fps) {
  task_queue_.PostTask([this, max_fps] {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (max_fps < std::numeric_limits<int>::max()) {
      wanted_fps_.emplace(max_fps);
    } else {
      wanted_fps_.reset();
    }
  });
}

void FrameGeneratorCapturer::ForceFrame() {
  // One-time non-repeating task,
  task_queue_.PostTask([this] {
    RTC_DCHECK_RUN_ON(&task_queue_);
    InsertFrame();
  });
}

TimeDelta FrameGeneratorCapturer::GetCurrentFrameInterval() {
  if (wanted_fps_ && *wanted_fps_ < target_capture_fps_)
    return TimeDelta::seconds(1) / *wanted_fps_;
  return TimeDelta::seconds(1) / target_capture_fps_;
}

}  // namespace test
}  // namespace webrtc
