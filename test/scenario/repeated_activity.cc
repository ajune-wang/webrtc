/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/repeated_activity.h"

namespace webrtc {
namespace test {

RepeatedActivity::RepeatedActivity(TimeDelta interval,
                                   std::function<void(TimeDelta)> function)
    : interval_(interval), function_(function) {}
RepeatedActivity::~RepeatedActivity() = default;

void RepeatedActivity::Stop() {
  interval_ = TimeDelta::PlusInfinity();
}

void RepeatedActivity::Poll(Timestamp time) {
  RTC_DCHECK(last_update_.IsFinite());
  if (time >= last_update_ + interval_) {
    function_(time - last_update_);
    last_update_ = time;
  }
}

void RepeatedActivity::SetStartTime(Timestamp time) {
  last_update_ = time;
}

Timestamp RepeatedActivity::NextTime() {
  RTC_DCHECK(last_update_.IsFinite());
  return last_update_ + interval_;
}

}  // namespace test
}  // namespace webrtc
