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

#include "call/adaptation/resource_adaptation_processor.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

const char kQualityScalerResourcePreventAdaptation[] =
    "WebRTC-QpScalerPreventAdaptUp";

QualityScalerResource::QualityScalerResource(
    ResourceAdaptationProcessor* adaptation_processor)
    : adaptation_processor_(adaptation_processor),
      quality_scaler_(nullptr),
      should_increase_frequency_(false),
      is_prevent_adapt_up_enabled_(!webrtc::field_trial::IsDisabled(
          kQualityScalerResourcePreventAdaptation)) {}

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

void QualityScalerResource::AdaptUp(VideoAdaptationReason reason) {
  RTC_DCHECK_EQ(reason, VideoAdaptationReason::kQuality);
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
}

bool QualityScalerResource::AdaptDown(VideoAdaptationReason reason) {
  RTC_DCHECK_EQ(reason, VideoAdaptationReason::kQuality);
  should_increase_frequency_ = false;
  OnResourceUsageStateMeasured(ResourceUsageState::kOveruse);
  // |should_increase_frequency_| may be set by OnAdaptationApplied(), triggered
  // by the ResourceAdaptationProcessor in response to by
  // OnResourceUsageStateMeasured().
  // TODO(hbos): In order to support asynchronicity (separating adaptation
  // processing from the encoder queue to support multi-stream), tell the
  // QualityScaler to increase frequency with a callback instead!
  printf("===> AdaptDown actually returns: %s\n",
         !should_increase_frequency_ ? "true" : "false");
  return !should_increase_frequency_;
}

void QualityScalerResource::OnAdaptationApplied(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    const Resource& reason_resource) {
  if (this == &reason_resource) {
    if (restrictions_after < restrictions_before) {
      // AdaptDown
      ++adaptations_;
    } else {
      // AdaptUp
      --adaptations_;
      RTC_DCHECK_GE(adaptations_, 0);
    }
  }

  if (adaptation_processor_->effective_degradation_preference() ==
          DegradationPreference::BALANCED &&
      DidDecreaseFrameRate(restrictions_before, restrictions_after)) {
    absl::optional<int> min_diff = BalancedDegradationSettings().MinFpsDiff(
        input_state.frame_size_pixels().value());
    if (min_diff && input_state.frames_per_second().value() > 0) {
      int fps_diff = input_state.frames_per_second().value() -
                     restrictions_after.max_frame_rate().value();
      if (fps_diff < min_diff.value()) {
        should_increase_frequency_ = true;
        printf("===> AdaptDown would have returned: %s\n",
               !should_increase_frequency_ ? "true" : "false");
      }
    }
  }
}

bool QualityScalerResource::IsAdaptationUpAllowed(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    const Resource& reason_resource) const {
  if (!is_prevent_adapt_up_enabled_ || !quality_scaler_) {
    return true;
  }
  bool adaptation_allowed = CheckQpOkForAdaptation();
  RTC_DCHECK(!(this == &reason_resource && !adaptation_allowed))
      << "We are rejecting our own scale up!";
  return adaptation_allowed;
}

bool QualityScalerResource::CheckQpOkForAdaptation() const {
  QualityScaler::QpFastFilterResult ff_result = quality_scaler_->QpFastFilter();
  switch (ff_result) {
    case QualityScaler::QpFastFilterResult::kLow:
      return true;
    case QualityScaler::QpFastFilterResult::kHigh:
      return false;
    case QualityScaler::QpFastFilterResult::kNotEnoughData:
    case QualityScaler::QpFastFilterResult::kMiddle:
      return adaptations_ == 0;
  }
}

}  // namespace webrtc
