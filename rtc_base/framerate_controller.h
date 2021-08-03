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

#include "absl/types/optional.h"

namespace rtc {

// Determines which frames in a sequence that should be dropped in order to meet
// the requested max framerate.
class FramerateController {
 public:
  FramerateController();
  ~FramerateController();

  // Sets the max framerate.
  // Default max framerate is maxint.
  void SetMaxFramerate(int max_framerate);

  // Returns true if the current frame should be dropped, false otherwise.
  // Input: timestamp of the frame in ns.
  bool DropFrame(int64_t timestamp_ns);

  void Reset();

 private:
  int max_framerate_;
  absl::optional<int64_t> next_frame_timestamp_ns_;
};

}  // namespace rtc

#endif  // RTC_BASE_FRAMERATE_CONTROLLER_H_
