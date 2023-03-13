/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/dvqa/pausable_state.h"

#include "api/units/timestamp.h"
#include "rtc_base/checks.h"

namespace webrtc {

void PausableState::Pause() {
  RTC_CHECK(!IsPaused());
  events_.push_back(Event{.time = clock_->CurrentTime(), .is_paused = true});
}

void PausableState::Resume() {
  RTC_CHECK(IsPaused());
  events_.push_back(Event{.time = clock_->CurrentTime(), .is_paused = false});
}

bool PausableState::IsPaused() const {
  return !events_.empty() && events_.back().is_paused;
}

bool PausableState::WasPausedAt(Timestamp time) const {
  if (events_.empty()) {
    return false;
  }

  size_t pos = GetPos(time);
  return pos != -1lu && events_[pos].is_paused;
}

bool PausableState::WasResumedAfter(Timestamp time) const {
  if (events_.empty()) {
    return false;
  }

  size_t pos = GetPos(time);
  return (pos + 1 < events_.size()) && !events_[pos + 1].is_paused;
}

size_t PausableState::GetPos(Timestamp time) const {
  size_t l = 0, r = events_.size() - 1;
  while (l < r) {
    size_t pos = (l + r) / 2;
    if (time < events_[pos].time) {
      r = pos;
    } else if (time >= events_[pos].time) {
      l = pos + 1;
    }
  }
  if (time < events_[l].time) {
    return l - 1;
  } else {
    return l;
  }
}

}  // namespace webrtc
