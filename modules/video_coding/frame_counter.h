/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_FRAME_COUNTER_H_
#define MODULES_VIDEO_CODING_FRAME_COUNTER_H_

#include <stdint.h>

#include <set>

namespace webrtc {

// Counts unique received timestamps.
class FrameCounter {
 public:
  FrameCounter() = default;
  FrameCounter(const FrameCounter&) = delete;
  FrameCounter& operator=(const FrameCounter&) = delete;
  ~FrameCounter() = default;

  void Add(uint32_t rtp_timestamp);

  // Returns number of different |rtp_timestamp| values passed to FrameCounter.
  int GetUniqueSeen() const { return unique_frames_seen_; }

 private:
  static constexpr int kMaxTimestampsHistory = 1000;

  int unique_frames_seen_ = 0;

  // Stores several last seen unique timestamps for quick search.
  std::set<uint32_t> history_set_;

  // The same unique timestamps in the circular buffer in the insertion order.
  uint32_t history_circular_buffer_[kMaxTimestampsHistory];
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_FRAME_COUNTER_H_
