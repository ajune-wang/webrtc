/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_VECTOR_FLOAT_FRAME_H_
#define MODULES_AUDIO_PROCESSING_AGC2_VECTOR_FLOAT_FRAME_H_

#include <vector>

#include "modules/audio_processing/include/float_audio_frame.h"

namespace webrtc {

// A construct consisting of a multi-channel audio frame, and a FloatFrame view
// of it.
class VectorFloatFrame {
 public:
  VectorFloatFrame(int num_channels,
                   int samples_per_channel,
                   float start_value);
  FloatAudioFrame<float> float_frame() { return float_frame_; }
  FloatAudioFrame<const float> float_frame() const { return float_frame_; }

  ~VectorFloatFrame();

 private:
  std::vector<std::vector<float>> channels_;
  std::vector<float*> channel_ptrs_;
  FloatAudioFrame<float> float_frame_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_VECTOR_FLOAT_FRAME_H_
