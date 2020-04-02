/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/quality_scaler_resource.h"

#include <utility>

#include "video/adaptation/resource_adaptation_processor.h"

namespace webrtc {

namespace {

bool DidDecreaseFrameRate(VideoSourceRestrictions restrictions_before,
                          VideoSourceRestrictions restrictions_after) {
  if (!restrictions_before.max_frame_rate().has_value()) {
    return restrictions_after.max_frame_rate().has_value();
  }
  if (!restrictions_after.max_frame_rate().has_value())
    return true;
  return restrictions_after.max_frame_rate().value() <
         restrictions_before.max_frame_rate().value();
}

}  // namespace

QualityScalerResource::QualityScalerResource(
    ResourceAdaptationProcessor* adaptation_processor)
    : quality_scaler_(nullptr),
      adaptation_processor_(adaptation_processor),
      last_adaptation_applied_response_(absl::nullopt) {}

bool QualityScalerResource::is_started() const {
  return quality_scaler_.get();
}

void QualityScalerResource::StartCheckForOveruse(
    VideoEncoder::QpThresholds qp_thresholds) {
  RTC_DCHECK(!is_started());
  quality_scaler_ =
      std::make_unique<QualityScaler>(this, std::move(qp_thresholds));
}

void QualityScalerResource::StopCheckForOveruse() {
  quality_scaler_.reset();
}

void QualityScalerResource::SetQpThresholds(
    VideoEncoder::QpThresholds qp_thresholds) {
  RTC_DCHECK(is_started());
  quality_scaler_->SetQpThresholds(std::move(qp_thresholds));
}

bool QualityScalerResource::QpFastFilterLow() {
  RTC_DCHECK(is_started());
  return quality_scaler_->QpFastFilterLow();
}

void QualityScalerResource::OnEncodeCompleted(const EncodedImage& encoded_image,
                                              int64_t time_sent_in_us) {
  if (quality_scaler_ && encoded_image.qp_ >= 0)
    quality_scaler_->ReportQp(encoded_image.qp_, time_sent_in_us);
}

void QualityScalerResource::OnFrameDropped(
    EncodedImageCallback::DropReason reason) {
  if (!quality_scaler_)
    return;
  switch (reason) {
    case EncodedImageCallback::DropReason::kDroppedByMediaOptimizations:
      quality_scaler_->ReportDroppedFrameByMediaOpt();
      break;
    case EncodedImageCallback::DropReason::kDroppedByEncoder:
      quality_scaler_->ReportDroppedFrameByEncoder();
      break;
  }
}

void QualityScalerResource::AdaptUp(AdaptReason reason) {
  RTC_DCHECK_EQ(reason, AdaptReason::kQuality);
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
}

bool QualityScalerResource::AdaptDown(AdaptReason reason) {
  RTC_DCHECK_EQ(reason, AdaptReason::kQuality);
  last_adaptation_applied_response_ = absl::nullopt;
  OnResourceUsageStateMeasured(ResourceUsageState::kOveruse);
  RTC_DCHECK(last_adaptation_applied_response_.has_value());
  return last_adaptation_applied_response_.value() !=
         ResourceListenerResponse::kQualityScalerShouldIncreaseFrequency;
}

void QualityScalerResource::DidApplyAdaptation(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    const Resource* reason_resource) {
  if (adaptation_processor_->effective_degradation_preference() ==
          DegradationPreference::BALANCED &&
      DidDecreaseFrameRate(restrictions_before, restrictions_after)) {
    absl::optional<int> min_diff = BalancedDegradationSettings().MinFpsDiff(
        input_state.frame_size_pixels().value());
    if (min_diff && input_state.frames_per_second().value() > 0) {
      int fps_diff = input_state.frames_per_second().value() -
                     restrictions_after.max_frame_rate().value();
      if (fps_diff < min_diff.value()) {
        last_adaptation_applied_response_ =
            ResourceListenerResponse::kQualityScalerShouldIncreaseFrequency;
        return;
      }
    }
  }
  last_adaptation_applied_response_ = ResourceListenerResponse::kNothing;
}

}  // namespace webrtc
