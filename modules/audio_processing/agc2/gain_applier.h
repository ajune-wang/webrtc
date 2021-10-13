/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_GAIN_APPLIER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_GAIN_APPLIER_H_

#include <stddef.h>

#include "modules/audio_processing/include/audio_frame_view.h"

namespace webrtc {

// Processes multi-channel audio by applying a fixed digital gain and then
// hard-clipping if requested. Allows to change the fixed gain.
class GainApplier {
 public:
  // Ctor. `gain_db` is the gain applied by `ApplyGain()`. If `hard_clip` is
  // true, `ApplyGain()` clamps `signal` after the fixed digital gain is
  // applied.
  GainApplier(float gain_db, bool hard_clip, int sample_rate_hz);

  // Detects and handles sample rate changes.
  void Initialize(int sample_rate_hz);

  void SetGainDb(float gain_db);

  float gain_db() const { return gain_db_; }

  // Applies the fixed gain to all the channels of `frame` and then, if
  // `hard_clip_` is true, clamps `signal` in the float S16 range. If the call
  // follows a `SetGain()` call, the gain is linearly ramped down/up over
  // `signal`.
  void ApplyGain(AudioFrameView<float> frame);

 private:
  const bool hard_clip_;
  float gain_db_;
  bool apply_gain_;
  float last_gain_factor_;
  float current_gain_factor_;
  float inverse_samples_per_channel_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_GAIN_APPLIER_H_
