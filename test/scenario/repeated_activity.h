/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_SCENARIO_REPEATED_ACTIVITY_H_
#define TEST_SCENARIO_REPEATED_ACTIVITY_H_

#include <functional>
#include <utility>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {
namespace test {

class RepeatedActivity {
 public:
  RepeatedActivity(TimeDelta interval, std::function<void(TimeDelta)> function);
  ~RepeatedActivity();

  void Stop();
  void Poll(Timestamp time);
  void SetStartTime(Timestamp time);
  Timestamp NextTime();

 private:
  TimeDelta interval_;
  std::function<void(TimeDelta)> function_;
  Timestamp last_update_ = Timestamp::MinusInfinity();
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_REPEATED_ACTIVITY_H_
