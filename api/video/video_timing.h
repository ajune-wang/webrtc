/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_TIMING_H_
#define API_VIDEO_VIDEO_TIMING_H_

#include <stdint.h>

#include <limits>
#include <string>

#include "absl/types/optional.h"
#include "api/units/time_delta.h"

namespace webrtc {

// Video timing timestamps in ms counted from capture_time_ms of a frame.
// This structure represents data sent in video-timing RTP header extension.
struct VideoSendTiming {
  enum TimingFrameFlags : uint8_t {
    kNotTriggered = 0,  // Timing info valid, but not to be transmitted.
                        // Used on send-side only.
    kTriggeredByTimer = 1 << 0,  // Frame marked for tracing by periodic timer.
    kTriggeredBySize = 1 << 1,   // Frame marked for tracing due to size.
    kInvalid = std::numeric_limits<uint8_t>::max()  // Invalid, ignore!
  };

  // Returns |time_ms - base_ms| capped at max 16-bit value.
  // Used to fill this data structure as per
  // https://webrtc.org/experiments/rtp-hdrext/video-timing/ extension stores
  // 16-bit deltas of timestamps from packet capture time.
  static uint16_t GetDeltaCappedMs(int64_t base_ms, int64_t time_ms);
  static uint16_t GetDeltaCappedMs(TimeDelta delta);

  uint16_t encode_start_delta_ms;
  uint16_t encode_finish_delta_ms;
  uint16_t packetization_finish_delta_ms;
  uint16_t pacer_exit_delta_ms;
  uint16_t network_timestamp_delta_ms;
  uint16_t network2_timestamp_delta_ms;
  uint8_t flags = TimingFrameFlags::kInvalid;
};

// Used to report precise timings of a 'timing frames'. Contains all important
// timestamps for a lifetime of that specific frame. Reported as a string via
// GetStats(). Only frame which took the longest between two GetStats calls is
// reported.
struct TimingFrameInfo {
  TimingFrameInfo();

  // Returns end-to-end delay of a frame, if sender and receiver timestamps are
  // synchronized, -1 otherwise.
  int64_t EndToEndDelay() const;

  // Returns true if current frame took longer to process than `other` frame.
  // If other frame's clocks are not synchronized, current frame is always
  // preferred.
  bool IsLongerThan(const TimingFrameInfo& other) const;

  // Returns true if flags are set to indicate this frame was marked for tracing
  // due to the size being outside some limit.
  bool IsOutlier() const;

  // Returns true if flags are set to indicate this frame was marked fro tracing
  // due to cyclic timer.
  bool IsTimerTriggered() const;

  // Returns true if the timing data is marked as invalid, in which case it
  // should be ignored.
  bool IsInvalid() const;

  std::string ToString() const;

  bool operator<(const TimingFrameInfo& other) const;

  bool operator<=(const TimingFrameInfo& other) const;

  uint32_t rtp_timestamp;  // Identifier of a frame.
  // All timestamps below are in local monotonous clock of a receiver.
  // If sender clock is not yet estimated, sender timestamps
  // (capture_time_ms ... pacer_exit_ms) are negative values, still
  // relatively correct.
  int64_t capture_time_ms;          // Captrue time of a frame.
  int64_t encode_start_ms;          // Encode start time.
  int64_t encode_finish_ms;         // Encode completion time.
  int64_t packetization_finish_ms;  // Time when frame was passed to pacer.
  int64_t pacer_exit_ms;  // Time when last packet was pushed out of pacer.
  // Two in-network RTP processor timestamps: meaning is application specific.
  int64_t network_timestamp_ms;
  int64_t network2_timestamp_ms;
  int64_t receive_start_ms;   // First received packet time.
  int64_t receive_finish_ms;  // Last received packet time.
  int64_t decode_start_ms;    // Decode start time.
  int64_t decode_finish_ms;   // Decode completion time.
  int64_t render_time_ms;     // Proposed render time to insure smooth playback.

  uint8_t flags;  // Flags indicating validity and/or why tracing was triggered.
};

// Minimum and maximum playout delay values from capture to render.
// These are best effort values.
//
// A value < 0 indicates no change from previous valid value.
//
// min = max = 0 indicates that the receiver should try and render
// frame as soon as possible.
//
// min = x, max = y indicates that the receiver is free to adapt
// in the range (x, y) based on network jitter.
class VideoPlayoutDelay {
 public:
  static constexpr TimeDelta kMax = 0xfff * TimeDelta::Millis(10);

  VideoPlayoutDelay() = default;
  constexpr VideoPlayoutDelay(TimeDelta min, TimeDelta max)
      : deprecated_min_ms(min.ms()), deprecated_max_ms(max.ms()) {
    RTC_DCHECK_LE(TimeDelta::Zero(), min);
    RTC_DCHECK_LE(min, max);
    RTC_DCHECK_LE(max, kMax);
  }
  [[deprecated]] VideoPlayoutDelay(int min_ms, int max_ms)
      : min_ms(min_ms), max_ms(max_ms) {}
  VideoPlayoutDelay(const VideoPlayoutDelay&) = default;
  VideoPlayoutDelay& operator=(const VideoPlayoutDelay&) = default;

  bool Set(TimeDelta min, TimeDelta max);

  TimeDelta min() const { return TimeDelta::Millis(deprecated_min_ms); }
  TimeDelta max() const { return TimeDelta::Millis(deprecated_max_ms); }

  friend bool operator==(const VideoPlayoutDelay& lhs,
                         const VideoPlayoutDelay& rhs) {
    return lhs.deprecated_min_ms == rhs.deprecated_min_ms &&
           lhs.deprecated_max_ms == rhs.deprecated_max_ms;
  }

  friend bool operator!=(const VideoPlayoutDelay& lhs,
                         const VideoPlayoutDelay& rhs) {
    return !(lhs == rhs);
  }

  // TODO(bugs.webrtc.org/13756): Remove when members become private and
  // thus invariants can be enforced by the setter.
  bool Valid() const {
    return TimeDelta::Zero() <= min() && min() <= max() && max() <= kMax;
  }

  // TODO(bugs.webrtc.org/13756): Make private and convert into TimeDelta.
  union {
    [[deprecated("Use accessors")]] int min_ms = -1;
    int deprecated_min_ms;
  };
  union {
    [[deprecated("Use accessors")]] int max_ms = -1;
    int deprecated_max_ms;
  };
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_TIMING_H_
