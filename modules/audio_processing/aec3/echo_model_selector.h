/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_MODEL_SELECTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_MODEL_SELECTOR_H_

#include <cstddef>
#include <limits>

#include "rtc_base/constructormagic.h"

namespace webrtc {

class EchoModelSelector {
 public:
  EchoModelSelector();
  void Reset();
  void Update(bool echo_saturation,
              bool converged_filter,
              bool diverged_filter,
              size_t blocks_with_proper_filter_adaptation,
              size_t capture_blocks_counter);
  bool LinearModelSelected() const { return linear_model_selected_; }

 private:
  size_t blocks_since_converged_filter_ =
      std::numeric_limits<std::size_t>::max();
  bool linear_model_selected_ = false;
  size_t diverge_counter_ = 0;
  RTC_DISALLOW_COPY_AND_ASSIGN(EchoModelSelector);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ECHO_MODEL_SELECTOR_H_
