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

#include "modules/audio_processing/agc2/gain_applier.h"
#include "modules/audio_processing/agc2/vad_with_level.h"
#include "modules/audio_processing/include/audio_frame_view.h"

namespace webrtc {

class ApmDataDumper;

// Part of the adaptive digital controller that sets a continuously updated
// target digital gain, determines how to change the gain towards the target and
// that applies such a gain.
class AdaptiveDigitalGainApplier {
 public:
  // Information about a frame to process.
  struct FrameInfo {
    float input_level_dbfs;        // Estimated speech plus noise level.
    float input_noise_level_dbfs;  // Estimated noise level.
    VadLevelAnalyzer::Result vad_result;
    float limiter_envelope_dbfs;  // Envelope level from the limiter.
    bool estimate_is_confident;
  };

  explicit AdaptiveDigitalGainApplier(ApmDataDumper* apm_data_dumper);
  AdaptiveDigitalGainApplier(const AdaptiveDigitalGainApplier&) = delete;
  AdaptiveDigitalGainApplier& operator=(const AdaptiveDigitalGainApplier&) =
      delete;

  // Analyzes `info` and processes `frame`.
  void Process(const FrameInfo& info, AudioFrameView<float> frame);

 private:
  ApmDataDumper* const apm_data_dumper_;
  GainApplier gain_applier_;

  int calls_since_last_gain_log_;
  bool gain_increase_allowed_;
  float last_gain_db_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_DIGITAL_GAIN_APPLIER_H_
