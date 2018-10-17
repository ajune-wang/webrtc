/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_LOW_CUT_FILTER_H_
#define MODULES_AUDIO_PROCESSING_LOW_CUT_FILTER_H_

#include <stddef.h>  // for size_t
#include <memory>    // for unique_ptr
#include <vector>    // for vector

#include "modules/audio_processing/audio_buffer.h"  // for AudioBuffer
#include "rtc_base/constructormagic.h"              // for RTC_DISALLOW_IMPL...

namespace webrtc {

class LowCutFilter {
 public:
  LowCutFilter(size_t channels, int sample_rate_hz);
  ~LowCutFilter();
  void Process(AudioBuffer* audio);

 private:
  class BiquadFilter;

  std::vector<std::unique_ptr<BiquadFilter>> filters_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(LowCutFilter);
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_LOW_CUT_FILTER_H_
