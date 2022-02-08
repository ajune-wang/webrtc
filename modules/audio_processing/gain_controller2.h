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

#include "absl/types/optional.h"
#include "modules/audio_processing/agc2/adaptive_digital_gain_controller.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/gain_applier.h"
#include "modules/audio_processing/agc2/limiter.h"
#include "modules/audio_processing/agc2/vad_wrapper.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

class AudioBuffer;

// Gain Controller 2 aims to automatically adjust levels by acting on the
// microphone gain and/or applying digital gain.
class GainController2 {
 public:
  GainController2(const AudioProcessing::Config::GainController2& config,
                  int sample_rate_hz,
                  int num_channels);
  GainController2(const GainController2&) = delete;
  GainController2& operator=(const GainController2&) = delete;
  ~GainController2();

  // Detects and handles changes of sample rate and/or number of channels.
  void Initialize(int sample_rate_hz, int num_channels);

  // Sets the fixed digital gain.
  void SetFixedGainDb(float gain_db);

  // Analyzes `audio`. This method and `Process()` are kept separate so that it
  // is possible to perform the analyses on a different tap point (e.g., before
  // transient suppression). Returns the speech probability for `audio`, that is
  // a value in [0, 1] - the higher, the more likelihood that speech is present.
  // Returns an unspecified value if, due to the configuration provided at
  // construction time, voice activity detection is disabled.
  absl::optional<float> Analyze(const AudioBuffer* audio);

  // Applies fixed and adaptive digital gains to `audio` and runs a limiter.
  // Calls `Analyze()` if it has not been called since the last call to
  // `Process()`.
  void Process(AudioBuffer* audio);

  // Handles analog level changes.
  void NotifyAnalogLevel(int level);

  static bool Validate(const AudioProcessing::Config::GainController2& config);

 private:
  static int instance_count_;
  const AvailableCpuFeatures cpu_features_;
  ApmDataDumper data_dumper_;
  GainApplier fixed_gain_applier_;
  std::unique_ptr<VoiceActivityDetectorWrapper> vad_;
  std::unique_ptr<AdaptiveDigitalGainController> adaptive_digital_controller_;
  Limiter limiter_;
  absl::optional<float> speech_probability_;
  int calls_since_last_limiter_log_;
  int analog_level_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_GAIN_CONTROLLER2_H_
