/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_CLOCK_DRIFT_DETECTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_CLOCK_DRIFT_DETECTOR_H_

#include <vector>

#include "api/array_view.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ClockDriftDetector {
 public:
  ClockDriftDetector();
  ~ClockDriftDetector();

  void Reset();
  void Analyze(size_t block_index, rtc::ArrayView<const float> filter);
  bool DriftDetected() const { return drift_detected_; };

 private:
  const size_t min_num_data_points_;
  bool drift_detected_ = false;

  std::vector<int> peak_index_buffer_;
  std::vector<size_t> block_index_buffer_;

  size_t buffer_index_ = 0;

  float ComputeDrift() const;
  void AddPeak(size_t block_index, size_t peak_index);

  RTC_DISALLOW_COPY_AND_ASSIGN(ClockDriftDetector);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_CLOCK_DRIFT_DETECTOR_H_
