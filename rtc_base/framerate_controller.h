/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FRAMERATE_CONTROLLER_H_
#define RTC_BASE_FRAMERATE_CONTROLLER_H_

#include <stdint.h>

#include "api/units/timestamp.h"

namespace rtc {

// Determines which frames that should be dropped based on input framerate and
// requested framerate.
class FramerateController {
 public:
  FramerateController();
  ~FramerateController();

  // Sets max framerate (default is maxdouble).
  void SetMaxFramerate(double max_framerate);

  // Returns true if the frame should be dropped, false otherwise.
  bool ShouldDropFrame(webrtc::Timestamp timestamp);

  void Reset();

 private:
  double max_framerate_;
  webrtc::Timestamp next_frame_timestamp_us_ =
      webrtc::Timestamp::MinusInfinity();
};

}  // namespace rtc

#endif  // RTC_BASE_FRAMERATE_CONTROLLER_H_
