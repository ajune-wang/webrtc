/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_AUDIBILITY_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_AUDIBILITY_H_

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/stationarity_estimator.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;

class EchoAudibility {
 public:
  EchoAudibility();
  ~EchoAudibility();

  void Reset();

  void Update(const RenderBuffer& render_buffer,
              size_t delay_blocks,
              size_t capture_block_counter_,
              const std::array<float, kBlockSize>& s);

  float GetResidualEchoScaling(int band) const {
    if (render_stationarity_.IsBandStationaryAhead(band)) {
      return 0.f;
    } else {
      return 1.0;
    }
  }

 private:
  // Compute the residual scaling per frequency for the current frame.
  void ComputeResidualScaling();
  size_t render_block_number_;
  StationarityEstimator render_stationarity_;
  // size_t num_nonaudible_blocks_ = 0;
  // size_t low_farend_counter_ = 0;
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  RTC_DISALLOW_COPY_AND_ASSIGN(EchoAudibility);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ECHO_AUDIBILITY_H_
