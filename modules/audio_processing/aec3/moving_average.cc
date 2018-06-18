
/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/moving_average.h"

#include <algorithm>
#include <functional>

namespace webrtc {
namespace aec3 {

MovingAverage::MovingAverage(size_t num_elem, size_t mem_len)
    : memory_(num_elem * mem_len, 0.f),
      num_elem_(num_elem),
      mem_len_(mem_len),
      mem_index_(0),
      scaling_(1.0f / static_cast<float>(mem_len)) {}

MovingAverage::~MovingAverage() = default;

void MovingAverage::Average(rtc::ArrayView<const float> input,
                            rtc::ArrayView<float> output) {
  // Copy input to memory.
  size_t i = mem_index_ * num_elem_;
  for (size_t k = 0; k < num_elem_; k++) {
    memory_[i + k] = input[k];
  }

  // Sum all contributions.
  std::copy(memory_.begin(), memory_.begin() + num_elem_, output.begin());
  for (auto i = memory_.begin() + num_elem_; i < memory_.end();
       i += num_elem_) {
    std::transform(i, i + num_elem_, output.begin(), output.begin(),
                   std::plus<float>());
  }

  // Divide by mem_len_.
  for (float& o : output) {
    o *= scaling_;
  }

  mem_index_ = (mem_index_ + 1) % mem_len_;
}

}  // namespace aec3
}  // namespace webrtc
