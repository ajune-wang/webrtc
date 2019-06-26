/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/buffer_level_filter.h"

#include <stdint.h>
#include <algorithm>

#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

BufferLevelFilter::BufferLevelFilter() {
  Reset();
}

void BufferLevelFilter::Reset() {
  filtered_current_level_ = 0;
  level_factor_ = 0.984;
}

void BufferLevelFilter::Update(int buffer_size_samples,
                               int time_stretched_samples) {
  filtered_current_level_ = level_factor_ * filtered_current_level_ +
                            (1 - level_factor_) * buffer_size_samples;

  filtered_current_level_ =
      std::max(0.0, filtered_current_level_ - time_stretched_samples);
}

void BufferLevelFilter::SetTargetBufferLevel(int target_buffer_level) {
  if (target_buffer_level <= 1) {
    level_factor_ = 0.980;
  } else if (target_buffer_level <= 3) {
    level_factor_ = 0.984;
  } else if (target_buffer_level <= 7) {
    level_factor_ = 0.988;
  } else {
    level_factor_ = 0.992;
  }
}

int BufferLevelFilter::filtered_current_level() const {
  return static_cast<int>(filtered_current_level_);
}

}  // namespace webrtc
