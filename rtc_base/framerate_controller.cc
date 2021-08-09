/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/framerate_controller.h"

#include <limits>

#include "api/units/time_delta.h"
#include "rtc_base/time_utils.h"

namespace rtc {
namespace {
constexpr double kMinFramerate = 0.5;
}  // namespace

FramerateController::FramerateController()
    : max_framerate_(std::numeric_limits<double>::max()) {}

FramerateController::~FramerateController() {}

void FramerateController::SetMaxFramerate(double max_framerate) {
  max_framerate_ = max_framerate;
}

bool FramerateController::ShouldDropFrame(webrtc::Timestamp timestamp) {
  if (max_framerate_ < kMinFramerate)
    return true;

  // If `max_framerate_` is not set (i.e. maxdouble), `frame_interval_ns` is
  // rounded to 0.
  webrtc::TimeDelta frame_interval_us =
      webrtc::TimeDelta::Micros(rtc::kNumMicrosecsPerSec / max_framerate_);
  if (frame_interval_us <= webrtc::TimeDelta::Zero()) {
    // Frame rate throttling not enabled.
    return false;
  }

  if (!next_frame_timestamp_us_.IsMinusInfinity()) {
    // Time until next frame should be outputted.
    const webrtc::TimeDelta time_until_next_frame_us =
        next_frame_timestamp_us_ - timestamp;
    // Continue if timestamp is within expected range.
    if (time_until_next_frame_us.Abs() < 2 * frame_interval_us) {
      // Drop if a frame shouldn't be outputted yet.
      if (time_until_next_frame_us > webrtc::TimeDelta::Zero())
        return true;
      // Time to output new frame.
      next_frame_timestamp_us_ += frame_interval_us;
      return false;
    }
  }

  // First timestamp received or timestamp is way outside expected range, so
  // reset. Set first timestamp target to just half the interval to prefer
  // keeping frames in case of jitter.
  next_frame_timestamp_us_ = timestamp + frame_interval_us / 2;
  return false;
}

void FramerateController::Reset() {
  max_framerate_ = std::numeric_limits<double>::max();
  next_frame_timestamp_us_ = webrtc::Timestamp::MinusInfinity();
}

}  // namespace rtc
