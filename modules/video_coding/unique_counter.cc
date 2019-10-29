/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/unique_counter.h"

#include <stdint.h>
#include <set>

#include "rtc_base/checks.h"

namespace webrtc {

void UniqueCounter::Add(uint32_t value) {
  if (!index_.insert(value).second) {
    // Already known.
    return;
  }
  int index = unique_seen_ % kMaxHistory;
  if (unique_seen_ >= kMaxHistory) {
    index_.erase(latest_[index]);
  }
  latest_[index] = value;
  ++unique_seen_;
}

}  // namespace webrtc
