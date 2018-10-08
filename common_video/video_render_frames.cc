/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/video_render_frames.h"

#include <utility>

#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {
// Don't render frames with timestamp older than 500ms from now.
const int kOldRenderTimestampMS = 500;
// Don't render frames with timestamp more than 10s into the future.
const int kFutureRenderTimestampMS = 10000;

const uint32_t kEventMaxWaitTimeMs = 200;
const uint32_t kMinRenderDelayMs = 10;
const uint32_t kMaxRenderDelayMs = 500;
const size_t kMaxIncomingFramesBeforeLogged = 100;
const size_t kHistogramSize = 100;

uint32_t EnsureValidRenderDelay(uint32_t render_delay) {
  return (render_delay < kMinRenderDelayMs || render_delay > kMaxRenderDelayMs)
             ? kMinRenderDelayMs
             : render_delay;
}
}  // namespace

VideoRenderFrames::VideoRenderFrames(uint32_t render_delay_ms)
    : render_delay_ms_(EnsureValidRenderDelay(render_delay_ms)),
      histogram_(kHistogramSize, 0) {}

VideoRenderFrames::~VideoRenderFrames() {
  frames_dropped_ += incoming_frames_.size();
  RTC_HISTOGRAM_COUNTS_1000("WebRTC.Video.DroppedFrames.RenderQueue",
                            frames_dropped_);
  RTC_LOG(LS_INFO) << "WebRTC.Video.DroppedFrames.RenderQueue "
                   << frames_dropped_;
  char log_stream_buf[8 * 1024];
  rtc::SimpleStringBuilder log_stream(log_stream_buf);
  log_stream << "Render delta histogram: ";
  for (size_t i = 0; i < kHistogramSize; ++i) {
    log_stream << i << ":" << histogram_[i] << " ";
  }
  RTC_LOG(LS_INFO) << log_stream.str();
}

int32_t VideoRenderFrames::AddFrame(VideoFrame&& new_frame) {
  const int64_t time_now = rtc::TimeMillis();

  // Drop old frames only when there are other frames in the queue, otherwise, a
  // really slow system never renders any frames.
  if (!incoming_frames_.empty() &&
      new_frame.render_time_ms() + kOldRenderTimestampMS < time_now) {
    RTC_LOG(LS_WARNING) << "Too old frame, timestamp=" << new_frame.timestamp();
    ++frames_dropped_;
    return -1;
  }

  if (new_frame.render_time_ms() > time_now + kFutureRenderTimestampMS) {
    RTC_LOG(LS_WARNING) << "Frame too long into the future, timestamp="
                        << new_frame.timestamp();
    ++frames_dropped_;
    return -1;
  }

  if (new_frame.render_time_ms() < last_render_time_ms_) {
    RTC_LOG(LS_WARNING) << "Frame scheduled out of order, render_time="
                        << new_frame.render_time_ms()
                        << ", latest=" << last_render_time_ms_;
    // For more details, see bug:
    // https://bugs.chromium.org/p/webrtc/issues/detail?id=7253
    ++frames_dropped_;
    return -1;
  }

  last_render_time_ms_ = new_frame.render_time_ms();
  incoming_frames_.emplace_back(std::move(new_frame));

  if (incoming_frames_.size() > kMaxIncomingFramesBeforeLogged) {
    RTC_LOG(LS_WARNING) << "Stored incoming frames: "
                        << incoming_frames_.size();
  }
  return static_cast<int32_t>(incoming_frames_.size());
}

absl::optional<VideoFrame> VideoRenderFrames::FrameToRender() {
  absl::optional<VideoFrame> render_frame;
  // Get the newest frame that can be released for rendering.
  while (!incoming_frames_.empty() && TimeToNextFrameRelease() <= 0) {
    if (render_frame) {
      ++frames_dropped_;
    }
    render_frame = std::move(incoming_frames_.front());
    incoming_frames_.pop_front();
  }
  if (render_frame) {
    int64_t render_time_ms = rtc::TimeMillis();
    if (last_frame_returned_time_ms_ != -1) {
      int64_t render_delta_ms = render_time_ms - last_frame_returned_time_ms_;
      size_t bucket = render_delta_ms;
      if (bucket >= kHistogramSize) {
        bucket = kHistogramSize - 1;
      }
      ++histogram_[bucket];
    }
    last_frame_returned_time_ms_ = render_time_ms;
  }
  return render_frame;
}

uint32_t VideoRenderFrames::TimeToNextFrameRelease() {
  if (incoming_frames_.empty()) {
    return kEventMaxWaitTimeMs;
  }
  const int64_t time_to_release = incoming_frames_.front().render_time_ms() -
                                  render_delay_ms_ - rtc::TimeMillis();
  return time_to_release < 0 ? 0u : static_cast<uint32_t>(time_to_release);
}

bool VideoRenderFrames::HasPendingFrames() const {
  return !incoming_frames_.empty();
}

}  // namespace webrtc
