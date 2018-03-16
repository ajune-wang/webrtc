/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_RENDER_ACTIVITY_H_
#define MODULES_AUDIO_PROCESSING_AEC3_RENDER_ACTIVITY_H_

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;
class RenderActivity {
 public:
  explicit RenderActivity(const EchoCanceller3Config& config);
  void Reset();
  void Update(rtc::ArrayView<const float> x_aligned, bool saturated_capture);
  size_t ActiveBlock() const { return active_render_; }
  size_t NumActiveBlocks() const { return active_render_blocks_; }
  size_t NumActiveBlocksWithoutSaturation() const {
    return active_render_blocks_with_no_saturation_;
  }

 private:
  const float active_render_limit_;
  size_t active_render_blocks_with_no_saturation_ = 0;
  size_t active_render_blocks_ = 0;
  bool active_render_ = false;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderActivity);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_RENDER_ACTIVITY_H_
