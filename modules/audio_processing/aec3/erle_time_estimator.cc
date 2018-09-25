/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/erle_time_estimator.h"

#include <algorithm>
#include <memory>
#include <numeric>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

namespace {
constexpr float kEpsilon = 1e-3f;
constexpr float kX2BandEnergyThreshold = 44015068.0f;
constexpr int kErleHold = 100;
constexpr int kPointsToAccumulate = 6;
}  // namespace

ErleTimeEstimator::ErleTimeEstimator(float min_erle, float max_erle_lf)
    : min_erle_log2_(FastApproxLog2f(min_erle + kEpsilon)),
      max_erle_lf_log2(FastApproxLog2f(max_erle_lf + kEpsilon)) {
  Reset();
}

ErleTimeEstimator::~ErleTimeEstimator() = default;

void ErleTimeEstimator::Reset() {
  erle_time_inst_.Reset();
  erle_time_domain_log2_ = min_erle_log2_;
  hold_counter_time_domain_ = 0;
}

void ErleTimeEstimator::Update(rtc::ArrayView<const float> X2,
                               rtc::ArrayView<const float> Y2,
                               rtc::ArrayView<const float> E2,
                               bool converged_filter) {
  if (converged_filter) {
    // Computes ERLE over all frequency bins.
    const float X2_sum = std::accumulate(X2.begin(), X2.end(), 0.0f);
    if (X2_sum > kX2BandEnergyThreshold * X2.size()) {
      const float Y2_sum = std::accumulate(Y2.begin(), Y2.end(), 0.0f);
      const float E2_sum = std::accumulate(E2.begin(), E2.end(), 0.0f);
      if (erle_time_inst_.Update(Y2_sum, E2_sum)) {
        hold_counter_time_domain_ = kErleHold;
        erle_time_domain_log2_ +=
            0.1f * ((erle_time_inst_.GetInstErle_log2().value()) -
                    erle_time_domain_log2_);
        erle_time_domain_log2_ = rtc::SafeClamp(
            erle_time_domain_log2_, min_erle_log2_, max_erle_lf_log2);
      }
    }
  }
  --hold_counter_time_domain_;
  if (hold_counter_time_domain_ <= 0) {
    erle_time_domain_log2_ =
        std::max(min_erle_log2_, erle_time_domain_log2_ - 0.044f);
  }
  if (hold_counter_time_domain_ == 0) {
    erle_time_inst_.ResetAccumulators();
  }
}

void ErleTimeEstimator::Dump(
    const std::unique_ptr<ApmDataDumper>& data_dumper) const {
  data_dumper->DumpRaw("aec3_erle_time_domain_log2", ErleTimeDomainLog2());
  erle_time_inst_.Dump(data_dumper);
}

ErleTimeEstimator::ErleTimeInstantaneous::ErleTimeInstantaneous() {
  Reset();
}

ErleTimeEstimator::ErleTimeInstantaneous::~ErleTimeInstantaneous() = default;

bool ErleTimeEstimator::ErleTimeInstantaneous::Update(const float Y2_sum,
                                                      const float E2_sum) {
  bool update_estimates = false;
  E2_acum_ += E2_sum;
  Y2_acum_ += Y2_sum;
  num_points_++;
  if (num_points_ == kPointsToAccumulate) {
    if (E2_acum_ > 0.f) {
      update_estimates = true;
      erle_log2_ = FastApproxLog2f(Y2_acum_ / E2_acum_ + kEpsilon);
    }
    num_points_ = 0;
    E2_acum_ = 0.f;
    Y2_acum_ = 0.f;
  }

  if (update_estimates) {
    UpdateMaxMin();
    UpdateQualityEstimate();
  }
  return update_estimates;
}

void ErleTimeEstimator::ErleTimeInstantaneous::Reset() {
  ResetAccumulators();
  max_erle_log2_ = -10.f;  // -30 dB.
  min_erle_log2_ = 33.f;   // 100 dB.
  inst_quality_estimate_ = 0.f;
}

void ErleTimeEstimator::ErleTimeInstantaneous::ResetAccumulators() {
  erle_log2_ = absl::nullopt;
  inst_quality_estimate_ = 0.f;
  num_points_ = 0;
  E2_acum_ = 0.f;
  Y2_acum_ = 0.f;
}

void ErleTimeEstimator::ErleTimeInstantaneous::Dump(
    const std::unique_ptr<ApmDataDumper>& data_dumper) const {
  data_dumper->DumpRaw("aec3_erle_time_inst_log2",
                       erle_log2_ ? *erle_log2_ : -10.f);
  data_dumper->DumpRaw(
      "aec3_erle_time_quality",
      GetInstQualityEstimate() ? GetInstQualityEstimate().value() : 0.f);
  data_dumper->DumpRaw("aec3_erle_time_max_log2", max_erle_log2_);
  data_dumper->DumpRaw("aec3_erle_time_min_log2", min_erle_log2_);
}

void ErleTimeEstimator::ErleTimeInstantaneous::UpdateMaxMin() {
  RTC_DCHECK(erle_log2_);
  if (erle_log2_.value() > max_erle_log2_) {
    max_erle_log2_ = erle_log2_.value();
  } else {
    max_erle_log2_ -= 0.0004;  // Forget factor, approx 1dB every 3 sec.
  }

  if (erle_log2_.value() < min_erle_log2_) {
    min_erle_log2_ = erle_log2_.value();
  } else {
    min_erle_log2_ += 0.0004;  // Forget factor, approx 1dB every 3 sec.
  }
}

void ErleTimeEstimator::ErleTimeInstantaneous::UpdateQualityEstimate() {
  const float alpha = 0.07f;
  float quality_estimate = 0.f;
  RTC_DCHECK(erle_log2_);
  if (max_erle_log2_ > min_erle_log2_) {
    quality_estimate = (erle_log2_.value() - min_erle_log2_) /
                       (max_erle_log2_ - min_erle_log2_);
  }
  if (quality_estimate > inst_quality_estimate_) {
    inst_quality_estimate_ = quality_estimate;
  } else {
    inst_quality_estimate_ +=
        alpha * (quality_estimate - inst_quality_estimate_);
  }
}

}  // namespace webrtc
