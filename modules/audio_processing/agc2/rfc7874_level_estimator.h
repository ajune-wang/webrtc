/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RFC7874_LEVEL_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RFC7874_LEVEL_ESTIMATOR_H_

#include <vector>

#include "modules/audio_processing/agc2/biquad_filter.h"

namespace webrtc {

class ApmDataDumper;

// Audio level estimator based on the RFC 7874 recommendations (see [1]).
//
// "[...] the audio for WebRTC is not constrained to have a passband specified
// by G.712 and can in fact be sampled at any sampling rate from 8 to 48 kHz and
// higher.  For this reason, the level SHOULD be normalized by only considering
// frequencies above 300 Hz, regardless of the sampling rate used. [...] The
// RECOMMENDED filter for normalizing the signal energy is a second-order
// Butterworth filter with a 300 Hz cutoff frequency."
//
// [1] https://datatracker.ietf.org/doc/html/rfc7874#section-4
class Rfc7874AudioLevelEstimator {
 public:
  // Peak absolute value and energy of an audio frame.
  struct Levels {
    float peak;
    float energy;
  };

  Rfc7874AudioLevelEstimator(int sample_rate_hz,
                             ApmDataDumper* apm_data_dumper);
  Rfc7874AudioLevelEstimator(const Rfc7874AudioLevelEstimator&) = delete;
  Rfc7874AudioLevelEstimator& operator=(const Rfc7874AudioLevelEstimator&) =
      delete;
  ~Rfc7874AudioLevelEstimator() = default;

  // Sets a new sample rate.
  void Initialize(int sample_rate_hz);

  // Creates a filtered copy of `audio` according to the RFC 7874 section 4
  // recommendations and returns the measured levels.
  Levels GetLevels(rtc::ArrayView<const float> audio);

 private:
  ApmDataDumper* apm_data_dumper_;
  std::vector<float> buffer_;
  BiQuadFilter high_pass_filter_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RFC7874_LEVEL_ESTIMATOR_H_
