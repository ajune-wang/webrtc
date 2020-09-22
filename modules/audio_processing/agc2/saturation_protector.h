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

#include "absl/types/optional.h"
#include "modules/audio_processing/agc2/agc2_common.h"

namespace webrtc {

// Saturation protector state. Exposed publicly for check-pointing and restore
// ops.
struct SaturationProtectorState {
 private:
  // Ring buffer which only supports (i) push back and (ii) read oldest item.
  class RingBuffer {
   public:
    bool operator==(const SaturationProtectorState::RingBuffer& s) const;
    bool operator!=(const SaturationProtectorState::RingBuffer& s) const;

    void Reset();
    // Pushes back `v`. If the buffer is full, the oldest item is replaced.
    void PushBack(float v);
    // Returns the oldest item in the buffer. Returns an empty value if the
    // buffer is empty.
    absl::optional<float> Front() const;

   private:
    std::array<float, kPeakEnveloperBufferSize> buffer_;
    int next_ = 0;
    int size_ = 0;
  };

 public:
  bool operator==(const SaturationProtectorState& s) const;
  bool operator!=(const SaturationProtectorState& s) const;

  float margin_db;  // Recommended margin.
  RingBuffer peak_delay_buffer;
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
void UpdateSaturationProtectorState(float speech_peak_dbfs,
                                    float speech_level_dbfs,
                                    SaturationProtectorState& state);

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_SATURATION_PROTECTOR_H_
