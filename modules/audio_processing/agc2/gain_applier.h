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
  // Ctor. `gain_factor` is the linear gain applied by `ApplyGain()`. If
  // `hard_clip` is true, `ApplyGain()` clamps `signal` in the float S16 range
  // after the fixed digital gain is applied.
  // TODO(bugs.webrtc.org/7494): Switch from `gain_factor` to `gain_db`.
  GainApplier(float gain_factor, bool hard_clip, int sample_rate_hz);

  // Detects and handles sample rate changes.
  void Initialize(int sample_rate_hz);

  // Applies `current_gain_factor_` to all the channels of `signal` and then, if
  // `hard_clip_samples_` is true, clamps `signal` in the float S16 range.
  void ApplyGain(AudioFrameView<float> signal);

  // Sets the gain and, if the gain changes, the next call of `ApplyGain()`
  // linearly ramps up the digital gain from the previous to the new one in one
  // frame.
  // TODO(bugs.webrtc.org/7494): Switch from `gain_factor` to `gain_db`.
  void SetGainFactor(float gain_factor);

  // Gets the gain.
  // TODO(bugs.webrtc.org/7494): Return gain in dB.
  float GetGainFactor() const { return current_gain_factor_; }

 private:
  const bool hard_clip_;
  float last_gain_factor_;
  float current_gain_factor_;
  float inverse_samples_per_channel_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_GAIN_APPLIER_H_
