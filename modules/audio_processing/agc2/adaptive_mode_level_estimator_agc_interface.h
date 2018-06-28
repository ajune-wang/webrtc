/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_MODE_LEVEL_ESTIMATOR_AGC_INTERFACE_H_
#define MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_MODE_LEVEL_ESTIMATOR_AGC_INTERFACE_H_

#include <vector>

#include "modules/audio_processing/agc/agc.h"
#include "modules/audio_processing/agc2/adaptive_agc.h"
#include "modules/audio_processing/agc2/vad_with_level.h"

namespace webrtc {
class AdaptiveModeLevelEstimatorAgcInterface : public Agc {
 public:
  explicit AdaptiveModeLevelEstimatorAgcInterface(
      ApmDataDumper* apm_data_dumper);

  // Returns the proportion of samples in the buffer which are at full-scale
  // (and presumably clipped).
  float AnalyzePreproc(const int16_t* audio, size_t length) override;
  // |audio| must be mono; in a multi-channel stream, provide the first (usually
  // left) channel.
  void Process(const int16_t* audio,
               size_t length,
               int sample_rate_hz) override;

  // Retrieves the difference between the target RMS level and the current
  // signal RMS level in dB. Returns true if an update is available and false
  // otherwise, in which case |error| should be ignored and no action taken.
  bool GetRmsErrorDb(int* error) override;
  void Reset() override;

  int set_target_level_dbfs(int level) override;
  int target_level_dbfs() const override;
  float voice_probability() const override;

  AdaptiveAgc* GetAgc() {
    return &agc_;
  }

 private:
  AdaptiveAgc agc_;
  //VadWithLevel vad_;
  float target_level_dbfs_ = 0.f;
  //float latest_voice_probability_ = 0.f;
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_MODE_LEVEL_ESTIMATOR_AGC_INTERFACE_H_
