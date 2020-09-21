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

class SaturationProtector {
 public:
  explicit SaturationProtector(ApmDataDumper* apm_data_dumper);
  SaturationProtector(ApmDataDumper* apm_data_dumper,
                      float initial_saturation_margin_db,
                      float extra_saturation_margin_db);

  // Updates the margin by analyzing the estimated speech level
  // `speech_level_dbfs` and the peak power `speech_peak_dbfs` for an observed
  // frame which is reliably classified as "speech".
  void UpdateMargin(float speech_peak_dbfs, float speech_level_dbfs);

  // Returns latest computed margin.
  float GetMarginDb() const;

  void Reset();

  void DebugDumpEstimate() const;

 private:
  float GetDelayedPeakDbfs() const;

  ApmDataDumper* apm_data_dumper_;
  // Parameters.
  const float initial_saturation_margin_db_;
  const float extra_saturation_margin_db_;
  // State.
  float margin_db_;
  struct {  // Ring buffer which only supports push back and read.
    std::array<float, kPeakEnveloperBufferSize> buffer;
    int next;  // Where to write the next pushed value.
    int size;  // Number of elements (up to size of `buffer`).
  } peak_delay_buffer_;
  float max_peaks_dbfs_;
  int time_since_push_ms_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_SATURATION_PROTECTOR_H_
