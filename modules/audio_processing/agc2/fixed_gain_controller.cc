/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/fixed_gain_controller.h"

#include <algorithm>
#include <cmath>

#include "api/array_view.h"

#include "modules/audio_processing/agc2/agc2_common.h"

#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
float DbToLinear(float x) {
  return std::pow(10.0, x / 20.0);
}
}  // namespace

// int FixedGainController::instance_count_ = 0;

FixedGainController::FixedGainController(ApmDataDumper* apm_data_dumper)
    : apm_data_dumper_(apm_data_dumper) {
  RTC_DCHECK_LT(0.f, gain_to_apply_);
  RTC_DLOG(LS_INFO) << "Gain to apply: " << gain_to_apply_;
}

FixedGainController::~FixedGainController() = default;

void FixedGainController::SetGain(float gain_to_apply_db) {
  // RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  // Changes in gain_to_apply_ cause discontinuities. We assume
  // gain_to_apply_ is set in the beginning of the call. If it is
  // frequently changed, we should add interpolation between the
  // values.
  gain_to_apply_ = DbToLinear(gain_to_apply_db);
}

void FixedGainController::SetSampleRate(size_t sample_rate_hz) {
  // TODO(aleloi): propagate the new sample rate to the GainCurveApplier.
}

void FixedGainController::EnableLimiter(bool enable_limiter) {
  enable_limiter_ = enable_limiter;
}

void FixedGainController::Process(MutableFloatAudioFrame signal) {
  // RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  // Apply fixed digital gain; interpolate if necessary. One of the
  // planned usages of the FGC is to only use the limiter. In that
  // case, the gain would be 1.0. Not doing the multiplications speeds
  // it up considerably. Hence the check.
  if (gain_to_apply_ != 1.f) {
    for (size_t k = 0; k < signal.num_channels(); ++k) {
      rtc::ArrayView<float> channel_view = signal.channel(k);
      for (auto& sample : channel_view) {
        sample *= gain_to_apply_;
      }
    }
  }

  // Use the limiter (if configured to).
  if (enable_limiter_) {
    // TODO(aleloi): Process the signal with the GainCurveApplier.

    // Dump data for debug.
    const auto channel_view = signal.channel(0);
    apm_data_dumper_->DumpRaw("agc2_fixed_digital_gain_curve_applier",
                              channel_view.size(), channel_view.data());
  }

  // Hard-clipping.
  for (size_t k = 0; k < signal.num_channels(); ++k) {
    rtc::ArrayView<float> channel_view = signal.channel(k);
    for (auto& sample : channel_view) {
      sample = std::max(kMinSampleValue, std::min(sample, kMaxSampleValue));
    }
  }
}
}  // namespace webrtc
