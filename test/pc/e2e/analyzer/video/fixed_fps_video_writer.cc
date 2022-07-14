/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/fixed_fps_video_writer.h"

#include <cmath>

#include "absl/types/optional.h"
#include "api/units/time_delta.h"
#include "api/video/video_sink_interface.h"
#include "test/testsupport/video_frame_writer.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr TimeDelta kOneSecond = TimeDelta::Seconds(1);

}  // namespace

FixedFpsVideoWriter::FixedFpsVideoWriter(Clock* clock,
                                         test::VideoFrameWriter* video_writer,
                                         int fps)
    : inter_frame_interval_(kOneSecond / fps),
      clock_(clock),
      video_writer_(video_writer) {}

FixedFpsVideoWriter::~FixedFpsVideoWriter() {
  WriteFrame(absl::nullopt);
  if (last_frame_.has_value()) {
    video_writer_->WriteFrame(*last_frame_);
  }
}

void FixedFpsVideoWriter::OnFrame(const VideoFrame& frame) {
  WriteFrame(frame);
}

void FixedFpsVideoWriter::WriteFrame(absl::optional<VideoFrame> frame) {
  if (!last_frame_.has_value() && !frame.has_value()) {
    return;
  }

  Timestamp now = Now();
  if (!last_frame_.has_value()) {
    // We received the first frame for this stream.
    last_frame_ = frame;
    last_frame_time_ = now;
    return;
  }

  RTC_CHECK(last_frame_time_.IsFinite());
  if (last_frame_time_ > now) {
    // Option III
    RTC_CHECK_GE(last_frame_time_ - now, inter_frame_interval_ / 2);
    if (frame.has_value()) {
      last_frame_ = frame;
      last_frame_time_ = now;
    }
    return;
  }

  TimeDelta since_last_frame = now - last_frame_time_;
  if (since_last_frame < inter_frame_interval_) {
    // Option II
    if (since_last_frame < inter_frame_interval_ / 2) {
      if (frame.has_value()) {
        last_frame_ = frame;
        last_frame_time_ = now;
      }
      return;
    } else {
      video_writer_->WriteFrame(*last_frame_);
      last_frame_ = frame;
      last_frame_time_ = now;
      return;
    }
  }

  // Option I
  while (since_last_frame > inter_frame_interval_) {
    video_writer_->WriteFrame(*last_frame_);
    since_last_frame = since_last_frame - inter_frame_interval_;
  }
  if (since_last_frame < inter_frame_interval_ / 2) {
    if (frame.has_value()) {
      last_frame_ = frame;
      last_frame_time_ = now;
    }
  } else {
    video_writer_->WriteFrame(*last_frame_);
    last_frame_ = frame;
    last_frame_time_ = now - since_last_frame + inter_frame_interval_;
  }
}

Timestamp FixedFpsVideoWriter::Now() const {
  return clock_->CurrentTime();
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
