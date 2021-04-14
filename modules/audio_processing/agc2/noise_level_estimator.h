/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_NOISE_LEVEL_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AGC2_NOISE_LEVEL_ESTIMATOR_H_

#include "modules/audio_processing/agc2/signal_classifier.h"
#include "modules/audio_processing/include/audio_frame_view.h"

namespace webrtc {
class ApmDataDumper;

class NoiseLevelEstimator {
 public:
  NoiseLevelEstimator(ApmDataDumper* data_dumper);
  NoiseLevelEstimator(const NoiseLevelEstimator&) = delete;
  NoiseLevelEstimator& operator=(const NoiseLevelEstimator&) = delete;
  ~NoiseLevelEstimator();
  // Returns the estimated noise level in dBFS.
  float Analyze(const AudioFrameView<const float>& frame);

<<<<<<< HEAD   (48ae01 [Merge-91] Remove RTCRemoteInboundRtpStreamStats duplicate m)
 private:
  void Initialize(int sample_rate_hz);

  ApmDataDumper* const data_dumper_;
  int sample_rate_hz_;
  float min_noise_energy_;
  bool first_update_;
  float noise_energy_;
  int noise_energy_hold_counter_;
  SignalClassifier signal_classifier_;
};
=======
// Creates a noise level estimator based on stationarity detection.
std::unique_ptr<NoiseLevelEstimator> CreateStationaryNoiseEstimator(
    ApmDataDumper* data_dumper);

// Creates a noise level estimator based on noise floor detection.
std::unique_ptr<NoiseLevelEstimator> CreateNoiseFloorEstimator(
    ApmDataDumper* data_dumper);
>>>>>>> CHANGE (61982a AGC2 lightweight noise floor estimator)

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_NOISE_LEVEL_ESTIMATOR_H_
