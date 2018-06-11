/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_REVERB_MODELLING_H_
#define MODULES_AUDIO_PROCESSING_AEC3_REVERB_MODELLING_H_

#include "api/audio/echo_canceller3_config.h"

namespace webrtc {

class ReverbModelling {
 public:
  ReverbModelling();
  ~ReverbModelling();

  // Reset the state.
  void Reset();

  // Updates the reverberation contributions.
  void UpdateReverbContributions(rtc::ArrayView<const float> tail,
                                 float gain_tail,
                                 float reverb_decay);

  // Add the reverberation contributions to an input/output power spectrum.
  void AddReverb(rtc::ArrayView<const float> tail,
                 float gain_tail,
                 float reverb_decay,
                 std::array<float, kFftLengthBy2Plus1>* power_spectrum);

  // Returns the current power spectrum reverberation contributions.
  const std::array<float, kFftLengthBy2Plus1>& GetPowerSpectrum() const {
    return reverb_;
  }

 private:
  std::array<float, kFftLengthBy2Plus1> reverb_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_REVERB_MODELLING_H_
