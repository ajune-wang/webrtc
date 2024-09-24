/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_EVALUATOR_H_
#define VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_EVALUATOR_H_

#include <optional>

#include "api/video/video_frame.h"
#include "common_video/corruption_detection_message.h"
#include "video/corruption_detection/halton_frame_sampler.h"

namespace webrtc {

class FrameInstrumentationEvaluator {
 public:
  std::optional<double> GetCorruptionScore(CorruptionDetectionMessage message,
                                           VideoFrame decoded_frame);

 private:
  HaltonFrameSampler frame_sampler_;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_EVALUATOR_H_
