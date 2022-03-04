/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/inter_frame_delay.h"

#include "absl/types/optional.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"

namespace webrtc {

namespace {
constexpr Frequency k90kHz = Frequency::KiloHertz(90);
}

VCMInterFrameDelay::VCMInterFrameDelay() {
  Reset();
}

// Resets the delay estimate.
void VCMInterFrameDelay::Reset() {
  prev_wall_clock_ = absl::nullopt;
  prev_rtp_timestamp_ = 0;
}

// Calculates the delay of a frame with the given timestamp.
// This method is called when the frame is complete.
absl::optional<TimeDelta> VCMInterFrameDelay::CalculateDelay(
    uint32_t rtp_timestamp,
    Timestamp now) {
  if (!prev_wall_clock_) {
    // First set of data, initialization, wait for next frame.
    prev_wall_clock_ = now;
    prev_rtp_timestamp_ = rtp_timestamp;
    return TimeDelta::Zero();
  }

  // This will be -1 for backward wrap arounds and +1 for forward wrap arounds.
  int32_t wrap_arounds_since_prev = CheckForWrapArounds(rtp_timestamp);

  // Account for reordering in jitter variance estimate in the future?
  // Note that this also captures incomplete frames which are grabbed for
  // decoding after a later frame has been complete, i.e. real packet losses.
  if ((wrap_arounds_since_prev == 0 && rtp_timestamp < prev_rtp_timestamp_) ||
      wrap_arounds_since_prev < 0) {
    return absl::nullopt;
  }

  // Compute the compensated timestamp difference.
  int64_t d_rtp_ticks =
      rtp_timestamp +
      wrap_arounds_since_prev * (static_cast<int64_t>(1) << 32) -
      prev_rtp_timestamp_;
  TimeDelta dts = d_rtp_ticks / k90kHz;
  TimeDelta dt = now - *prev_wall_clock_;

  // frameDelay is the difference of dT and dTS -- i.e. the difference of the
  // wall clock time difference and the timestamp difference between two
  // following frames.
  TimeDelta delay = dt - dts;

  prev_rtp_timestamp_ = rtp_timestamp;
  prev_wall_clock_ = now;
  return delay;
}

// Investigates if the timestamp clock has overflowed since the last timestamp
// and keeps track of the number of wrap arounds since reset.
int VCMInterFrameDelay::CheckForWrapArounds(uint32_t rtp_timestamp) const {
  if (rtp_timestamp < prev_rtp_timestamp_) {
    // This difference will probably be less than -2^31 if we have had a wrap
    // around (e.g. timestamp = 1, _prevTimestamp = 2^32 - 1). Since it is cast
    // to a int32_t, it should be positive.
    if (static_cast<int32_t>(rtp_timestamp - prev_rtp_timestamp_) > 0) {
      // Forward wrap around.
      return 1;
    }
    // This difference will probably be less than -2^31 if we have had a
    // backward wrap around. Since it is cast to a int32_t, it should be
    // positive.
  } else if (static_cast<int32_t>(prev_rtp_timestamp_ - rtp_timestamp) > 0) {
    // Backward wrap around.
    return -1;
  }
  return 0;
}

}  // namespace webrtc
