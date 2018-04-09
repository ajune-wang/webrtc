/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_audibility.h"

#include <math.h>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"

namespace webrtc {
namespace {

constexpr size_t kMaxNumLookahead = 10;
constexpr int kNumBlocksOverhangStationary = 2 * kNumBlocksPerSecond;
}  // namespace

int EchoAudibility::instance_count_ = 0;

EchoAudibility::EchoAudibility()
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      inaudible_blocks_(kMaxNumLookahead + 1, true) {
  residual_echo_scaling_.fill(1.0f);
  capture_non_stationary_overhang_.fill(0);
}

EchoAudibility::~EchoAudibility() = default;

void EchoAudibility::AddOverhangCaptureStationarityStatus(size_t k) {
  if (capture_stationarity_.IsBandStationary(k)) {
    capture_non_stationary_overhang_[k] =
        std::max(capture_non_stationary_overhang_[k] - 1, 0);
  } else {
    capture_non_stationary_overhang_[k] = std::min(
        capture_non_stationary_overhang_[k] + 1, kNumBlocksOverhangStationary);
  }
}

void EchoAudibility::ComputeResidualScaling() {
  for (size_t k = 0; k < residual_echo_scaling_.size(); ++k) {
    float& r = residual_echo_scaling_[k];
    r = 1.0f;
    AddOverhangCaptureStationarityStatus(k);

    if (render_stationarity_.IsBandStationary(k) &&
        (capture_non_stationary_overhang_[k] > 0)) {
      float noise = render_stationarity_.GetStationarityPowerBand(k);
      float frame_power = render_stationarity_.GetPowerBandSmthSpectrum(k);
      if (noise != 0.f) {
        r = frame_power / (noise * 1000.f);
        r = std::min(1.f, r);
        r = std::max(0.f, r);
      }
    }
  }
}

void EchoAudibility::Update(const RenderBuffer& render_buffer,
                            size_t delay_blocks,
                            const std::array<float, kFftLengthBy2Plus1>& Y2,
                            const std::array<float, kBlockSize>& s) {
  size_t num_lookahead = render_buffer.Headroom() - delay_blocks;

  inaudible_blocks_.resize(std::min(kMaxNumLookahead + 1, num_lookahead + 1));
  inaudible_blocks_[0] =
      render_stationarity_.Update(render_buffer.Spectrum(delay_blocks));
  capture_stationarity_.Update(Y2);

  if (++convergence_counter_ < 20) {
    std::fill(inaudible_blocks_.begin(), inaudible_blocks_.end(), false);
  } else {
    for (size_t k = 1; k < inaudible_blocks_.size(); k++) {
      inaudible_blocks_[k] = render_stationarity_.Analyze(
          render_buffer.Spectrum(delay_blocks + k));
    }
  }

  auto& x = render_buffer.Block(-delay_blocks)[0];
  auto result_x = std::minmax_element(x.begin(), x.end());
  const float x_abs = std::max(fabsf(*result_x.first), fabsf(*result_x.second));
  low_farend_counter_ = x_abs < 100.f ? low_farend_counter_ + 1 : 0;

  if (++convergence_counter_ < 20 && !inaudible_blocks_[0]) {
    auto result_s = std::minmax_element(s.begin(), s.end());
    const float s_abs =
        std::max(fabsf(*result_s.first), fabsf(*result_s.second));
    inaudible_blocks_[0] = (s_abs < 30.f) || low_farend_counter_ > 20;
  }

  num_nonaudible_blocks_ = 0;
  while (num_nonaudible_blocks_ < inaudible_blocks_.size() &&
         inaudible_blocks_[num_nonaudible_blocks_]) {
    ++num_nonaudible_blocks_;
  }
  if (num_nonaudible_blocks_ > 0)
    printf("A:%zu\n", num_nonaudible_blocks_);

  ComputeResidualScaling();

  data_dumper_->DumpRaw("aec3_audibility_x", x);

  data_dumper_->DumpRaw("aec3_render_stationary_power",
                        render_stationarity_.GetStationaryPower());
  data_dumper_->DumpRaw("aec3_num_non_audible_echo_blocks",
                        num_nonaudible_blocks_);
  data_dumper_->DumpRaw("aec3_residual_echo_scaling", residual_echo_scaling_);
  data_dumper_->DumpRaw("aec3_audibility_isStationary",
                        (float)render_stationarity_.IsFrameStationary());
}

}  // namespace webrtc
