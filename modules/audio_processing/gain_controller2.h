/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_GAIN_CONTROLLER2_H_
#define MODULES_AUDIO_PROCESSING_GAIN_CONTROLLER2_H_

#include <memory>
#include <string>

#include "modules/audio_processing/agc2/adaptive_agc.h"
#include "modules/audio_processing/agc2/gain_applier.h"
#include "modules/audio_processing/agc2/limiter.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;
class AudioBuffer;

// Gain Controller 2 aims to automatically adjust levels by controlling the
// microphone gain and/or applying digital gains.
class GainController2 {
 public:
  GainController2();
  ~GainController2();

  void HandleCapturePreGainRuntimeSettings(float gain_factor);

  void Initialize(int sample_rate_hz);
  // Applies a pre-gain and returns true if a gain different from that of
  // the last call has been used - i.e., the echo path gain has changed.
  // The applied gain is linear and the signal is clipped (no soft-saturation
  // is used).
  // This method should be called at the beginning of the audio processing
  // pipeline, in particular before any software echo removal step and before
  // Process() is called.
  bool ApplyPreGain(AudioBuffer* audio);
  // Applies a post-gain. This method should be called after any software echo
  // removal step and after ApplyPreGain() is called.
  void ApplyPostGain(AudioBuffer* audio);
  void NotifyAnalogLevel(int level);

  void ApplyConfig(const AudioProcessing::Config::GainController2& config);
  static bool Validate(const AudioProcessing::Config::GainController2& config);
  static std::string ToString(
      const AudioProcessing::Config::GainController2& config);

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  AudioProcessing::Config::GainController2 config_;
  // Pre-processing components.
  GainApplier pre_fixed_digital_gain_applier_;
  // Post-processing components.
  GainApplier fixed_digital_gain_applier_;
  std::unique_ptr<AdaptiveAgc> adaptive_digital_controller_;
  Limiter limiter_;
  int analog_level_ = -1;
  bool fixed_pregain_has_changed_;

  RTC_DISALLOW_COPY_AND_ASSIGN(GainController2);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_GAIN_CONTROLLER2_H_
