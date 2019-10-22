/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_counter.h"

#include <set>
#include <vector>

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

constexpr int kMaxTimestampsHistory = 1000;

}  // namespace

FrameCounter::FrameCounter()
    : rtp_timestamps_history_queue_(kMaxTimestampsHistory) {}

void FrameCounter::Add(uint32_t rtp_timestamp) {
  if (!rtp_timestamps_history_set_.insert(rtp_timestamp).second) {
    // Already known timestamp.
    return;
  }
  if (unique_frames_seen_ < kMaxTimestampsHistory) {
    rtp_timestamps_history_queue_[unique_frames_seen_] = rtp_timestamp;
    RTC_DCHECK_EQ(rtp_timestamps_history_set_.size(), unique_frames_seen_ + 1);
  } else {
    int index = unique_frames_seen_ % kMaxTimestampsHistory;
    rtp_timestamps_history_set_.erase(rtp_timestamps_history_queue_[index]);
    rtp_timestamps_history_queue_[index] = rtp_timestamp;
    RTC_DCHECK_EQ(rtp_timestamps_history_set_.size(), kMaxTimestampsHistory);
  }
  ++unique_frames_seen_;
}

}  // namespace webrtc
