/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/fake_clock.h"

#include "rtc_base/checks.h"
#include "rtc_base/message_queue.h"

#include <iostream>

namespace rtc {

int64_t FakeClock::TimeNanos() const {
  CritScope cs(&lock_);
  printf("Return fake time as %li ns\n", time_);
  return time_;
}

void FakeClock::SetTimeNanos(int64_t nanos) {
  printf("Setting fake time to %li ns\n", nanos);
  {
    CritScope cs(&lock_);
    RTC_DCHECK(nanos >= time_);
    time_ = nanos;
  }
  // If message queues are waiting in a socket select() with a timeout provided
  // by the OS, they should wake up and dispatch all messages that are ready.
  MessageQueueManager::ProcessAllMessageQueuesForTesting();
}

void FakeClock::AdvanceTime(webrtc::TimeDelta delta) {
  printf("Advancing fake time with %i us\n", delta.us<int>());
  {
    CritScope cs(&lock_);
    time_ += delta.ns();
  }
  MessageQueueManager::ProcessAllMessageQueuesForTesting();
}

ScopedFakeClock::ScopedFakeClock() {
  printf("Entering scoped fake clock. \n");
  prev_clock_ = SetClockForTesting(this);
}

ScopedFakeClock::~ScopedFakeClock() {
  SetClockForTesting(prev_clock_);
  printf("Exiting scoped fake clock. \n");
}

}  // namespace rtc
