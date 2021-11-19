/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_DIGITAL_GAIN_APPLIER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_DIGITAL_GAIN_APPLIER_H_

#include <vector>

#include "modules/audio_processing/agc2/gain_applier.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

class ApmDataDumper;

// TODO(bugs.webrtc.org/7494): Split into `GainAdaptor` and `GainApplier`.
// Selects the target digital gain, decides when and how quickly to adapt to the
// target and applies the current gain to 10 ms frames.
class AdaptiveDigitalGainApplier {
 public:
  // Information about a frame to process.
  struct FrameInfo {
    float speech_probability;     // Probability of speech in the [0, 1] range.
    float speech_level_dbfs;      // Estimated speech level (dBFS).
    bool speech_level_reliable;   // True with reliable speech level estimation.
    float noise_rms_dbfs;         // Estimated noise RMS level (dBFS).
    float headroom_db;            // Headroom (dB).
    float limiter_envelope_dbfs;  // Envelope level from the limiter (dBFS).
  };

  // TODO(bugs.webrtc.org/7494): Remove when field trial overrides are removed.
  struct FastAdaptationConfig {
    bool disabled = false;
    // Noise level threshold below which a fast gain adaptation speed is used.
    float noise_level_threshold_dbfs = -75.0f;
    // Minimum observation period with noise level below the threshold to allow
    // fast gain adaptation.
    int hold_low_noise_ms = 1000;
    // Multiplier applied to the normal gain adaptation speed when the fast gain
    // adaptation is allowed.
    int max_gain_change_multiplier = 4;
  };

  AdaptiveDigitalGainApplier(
      ApmDataDumper* apm_data_dumper,
      const AudioProcessing::Config::GainController2::AdaptiveDigital& config,
      const FastAdaptationConfig& fast_adaptation_config,
      int sample_rate_hz,
      int num_channels);
  AdaptiveDigitalGainApplier(const AdaptiveDigitalGainApplier&) = delete;
  AdaptiveDigitalGainApplier& operator=(const AdaptiveDigitalGainApplier&) =
      delete;

  void Initialize(int sample_rate_hz, int num_channels);

  // Analyzes `info`, updates the digital gain and applies it to a 10 ms
  // `frame`. Supports any sample rate supported by APM.
  void Process(const FrameInfo& info, AudioFrameView<float> frame);

 private:
  // Returns true if fast adaptation is allowed.
  bool FastAdaptationAllowed(float noise_rms_dbfs);

  ApmDataDumper* const apm_data_dumper_;
  GainApplier gain_applier_;

  const AudioProcessing::Config::GainController2::AdaptiveDigital config_;
  const FastAdaptationConfig fast_adaptation_config_;
  const float max_gain_change_db_per_10ms_;
  const int hold_low_noise_num_frames_;

  int calls_since_last_gain_log_;
  int frames_to_gain_increase_allowed_;
  float last_gain_db_;
  int frames_to_low_noise_;

  std::vector<std::vector<float>> dry_run_frame_;
  std::vector<float*> dry_run_channels_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_DIGITAL_GAIN_APPLIER_H_
