/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_activity.h"

#include <numeric>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/checks.h"

namespace webrtc {

RenderActivity::RenderActivity(const EchoCanceller3Config& config)
    : active_render_limit_(config.render_levels.active_render_limit *
                           config.render_levels.active_render_limit *
                           kFftLengthBy2) {
  Reset();
}

void RenderActivity::Reset() {
  active_render_blocks_with_no_saturation_ = 0;
  active_render_blocks_ = 0;
  active_render_ = false;
}

void RenderActivity::Update(rtc::ArrayView<const float> x_aligned,
                            bool saturated_capture) {
  active_render_ =
      std::inner_product(x_aligned.begin(), x_aligned.end(), x_aligned.begin(),
                         0.f) > active_render_limit_;
  active_render_blocks_with_no_saturation_ +=
      active_render_ && !saturated_capture ? 1 : 0;
  active_render_blocks_ += active_render_ ? 1 : 0;
}

}  // namespace webrtc
