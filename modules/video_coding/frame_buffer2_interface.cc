/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_buffer2_interface.h"

#include <memory>

#include "modules/video_coding/frame_buffer2.h"
#include "modules/video_coding/frame_buffer2_adapter.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

std::unique_ptr<FrameBuffer2Interface> CreateFrameBuffer2FromFieldTrial(
    DecodeStreamTimeouts timeouts,
    Clock* clock,
    VCMTiming* timing,
    VCMReceiveStatisticsCallback* stats_callback) {
  if (field_trial::IsEnabled("WebRTC-UseFrameBuffer3")) {
    return std::make_unique<FrameBuffer2Adapter>(timeouts, clock, timing,
                                                 stats_callback);
  }
  return std::make_unique<video_coding::FrameBuffer>(timeouts, clock, timing,
                                                     stats_callback);
}

}  // namespace webrtc