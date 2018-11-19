/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_CLOCKDRIFT_DETECTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_CLOCKDRIFT_DETECTOR_H_

#include <array>

namespace webrtc {

class ApmDataDumper;
struct DownsampledRenderBuffer;
struct EchoCanceller3Config;

// Detects clockdrift by analyzing the estimated delay.
class ClockdriftDetector {
 public:
  enum class ClockdriftLevel { kNone, kProbable, kVerified };
  ClockdriftDetector();
  ~ClockdriftDetector();
  void Update(int lag);
  bool VerifiedClockdrift() { return level_ == ClockdriftLevel::kVerified; }
  bool ProbableClockdrift() { return level_ != ClockdriftLevel::kNone; }

 private:
  std::array<int, 3> lag_history_;
  ClockdriftLevel level_;
  size_t stability_counter;
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_CLOCKDRIFT_DETECTOR_H_
