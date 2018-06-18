/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_MOVING_AVERAGE_H_
#define MODULES_AUDIO_PROCESSING_AEC3_MOVING_AVERAGE_H_

#include <vector>

#include "api/array_view.h"

namespace webrtc {
namespace aec3 {

class MovingAverage {
 public:
  MovingAverage(size_t num_elem, size_t mem_len);
  ~MovingAverage();
  void Average(rtc::ArrayView<const float> input, rtc::ArrayView<float> output);

 private:
  std::vector<float> memory_;
  size_t num_elem_;
  size_t mem_len_;
  size_t mem_index_;
  float scaling_;
};

}  // namespace aec3
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_MOVING_AVERAGE_H_
