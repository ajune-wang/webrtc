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
  void Update(const RenderBuffer& render_buffer,
              size_t delay_blocks,
              const std::array<float, kBlockSize>& s);

  const std::array<float, kFftLengthBy2Plus1>& GetResidualEchoScaling() const {
    return residual_echo_scaling_;
  }
  size_t NumNonAudibleBlocks() const { return num_nonaudible_blocks_; }

 private:
  // Compute the residual scaling per frequency for the current frame.
  void ComputeResidualScaling();
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  StationarityEstimator render_stationarity_;
  std::vector<bool> inaudible_blocks_;
  size_t convergence_counter_ = 0;
  size_t num_nonaudible_blocks_ = 0;
  std::array<float, kFftLengthBy2Plus1> residual_echo_scaling_;
  size_t low_farend_counter_ = 0;
  RTC_DISALLOW_COPY_AND_ASSIGN(EchoAudibility);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ECHO_AUDIBILITY_H_
