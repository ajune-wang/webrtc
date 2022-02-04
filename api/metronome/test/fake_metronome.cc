/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/metronome/test/fake_metronome.h"

#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {

FakeMetronome::FakeMetronome(TimeDelta tick_period)
    : tick_period_(tick_period) {}

void FakeMetronome::AddListener(TickListener* listener) {
  listeners_.insert(listener);
}

void FakeMetronome::RemoveListener(TickListener* listener) {
  listeners_.erase(listener);
}
TimeDelta FakeMetronome::TickPeriod() const {
  return tick_period_;
}
void webrtc::FakeMetronome::Tick() {
  for (auto* listener : listeners_) {
    listener->OnTickTaskQueue()->PostTask(
        ToQueuedTask([listener] { listener->OnTick(); }));
  }
}
}  // namespace webrtc
