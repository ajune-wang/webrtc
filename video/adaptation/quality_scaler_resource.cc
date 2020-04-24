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

namespace webrtc {

QualityScalerResource::QualityScalerResource(
    ResourceAdaptationProcessor* adaptation_processor)
    : adaptation_processor_(adaptation_processor),
      quality_scaler_(nullptr),
      qp_usage_report_in_progress_(false),
      qp_usage_callback_should_clear_qp_samples_(true) {}

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

void QualityScalerResource::OnReportQpUsageHigh(
    rtc::scoped_refptr<QualityScalerQpUsageHandlerCallback> callback) {
  OnResourceUsageStateMeasured(ResourceUsageState::kOveruse);
  callback->OnQpUsageHandled(true);
}

void QualityScalerResource::OnReportQpUsageLow(
    rtc::scoped_refptr<QualityScalerQpUsageHandlerCallback> callback) {
  RTC_DCHECK(!qp_usage_report_in_progress_);
  qp_usage_report_in_progress_ = true;
  qp_usage_callback_should_clear_qp_samples_ = true;
  // If this triggers adaptation, OnAdaptationApplied() is called, which may
  // modify |qp_usage_callback_should_clear_qp_samples_|.
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
  callback->OnQpUsageHandled(qp_usage_callback_should_clear_qp_samples_);
  qp_usage_report_in_progress_ = false;
}

void QualityScalerResource::OnAdaptationApplied(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    const Resource& reason_resource) {
  // We only care about adaptations caused by the QualityScaler.
  if (!qp_usage_report_in_progress_)
    return;
  // TODO(hbos): Could we get rid of some of this complexity if the
  // QualityScaler simply implemented clearing QP samples at
  // OnAdaptationApplied(), regardless if it was the QualityScaler or a
  // different resource that had triggered the adaptation? It doesn't seem like
  // the act of clearing QP necessarily needs to be tied to a CheckQp() event.
  if (adaptation_processor_->effective_degradation_preference() ==
          DegradationPreference::BALANCED &&
      DidDecreaseFrameRate(restrictions_before, restrictions_after)) {
    absl::optional<int> min_diff = BalancedDegradationSettings().MinFpsDiff(
        input_state.frame_size_pixels().value());
    if (min_diff && input_state.frames_per_second().value() > 0) {
      int fps_diff = input_state.frames_per_second().value() -
                     restrictions_after.max_frame_rate().value();
      if (fps_diff < min_diff.value()) {
        qp_usage_callback_should_clear_qp_samples_ = false;
      }
    }
  }
}

}  // namespace webrtc
