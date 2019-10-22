/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_counter.h"

#include <stdint.h>

#include <set>

#include "rtc_base/checks.h"

namespace webrtc {

void FrameCounter::Add(uint32_t rtp_timestamp) {
  if (!history_set_.insert(rtp_timestamp).second) {
    // Already known timestamp.
    return;
  }
  int index = unique_frames_seen_ % kMaxTimestampsHistory;
  if (unique_frames_seen_ >= kMaxTimestampsHistory) {
    history_set_.erase(history_circular_buffer_[index]);
  }
  history_circular_buffer_[index] = rtp_timestamp;
  ++unique_frames_seen_;
}

}  // namespace webrtc
