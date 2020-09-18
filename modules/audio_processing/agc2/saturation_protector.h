/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_SATURATION_PROTECTOR_H_
#define MODULES_AUDIO_PROCESSING_AGC2_SATURATION_PROTECTOR_H_

#include <array>

#include "modules/audio_processing/agc2/agc2_common.h"

namespace webrtc {

class ApmDataDumper;

// Saturation protector state. Exposed publicly for check-pointing and restore
// ops.
struct SaturationProtectorState {
  bool operator==(const SaturationProtectorState& s) const;
  bool operator!=(const SaturationProtectorState& s) const;
  float margin_db;  // Recommended margin.
  struct {          // Ring buffer which only supports push back and read.
    std::array<float, kPeakEnveloperBufferSize> buffer;
    int next;           // Where to write the next pushed value.
    int size;           // Number of elements (up to size of `buffer`).
  } speech_peaks_dbfs;  // Ring buffer to store the recent speech peak powers.
  float max_peaks_dbfs;
  int time_since_push_ms;  // Time since the last ring buffer push operation.
};

// Resets the saturation protector state.
void ResetSaturationProtectorState(float initial_margin_db,
                                   SaturationProtectorState& state);

// Updates `state` by analyzing the estimated speech level `speech_level_dbfs`
// and the peak power `speech_peak_dbfs` for an observed frame which is
// reliably classified as "speech". `state` must not be modified without calling
// this function.
void UpdateSaturationProtectorState(float speech_level_dbfs,
                                    float speech_peak_dbfs,
                                    SaturationProtectorState& state);

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_SATURATION_PROTECTOR_H_
