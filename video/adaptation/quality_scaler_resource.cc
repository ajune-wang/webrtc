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
      qp_usage_callback_should_clear_qp_samples_(false) {}

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
  OnReportQpUsageHighImpl();
  callback->OnQpUsageHandled(qp_usage_callback_should_clear_qp_samples_);
}

bool QualityScalerResource::TriggerQpUsageHighForTesting() {
  OnReportQpUsageHighImpl();
  return qp_usage_callback_should_clear_qp_samples_;
}

void QualityScalerResource::OnReportQpUsageLow(
    rtc::scoped_refptr<QualityScalerQpUsageHandlerCallback> callback) {
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
  callback->OnQpUsageHandled(true);
}

void QualityScalerResource::OnReportQpUsageHighImpl() {
  RTC_DCHECK(!qp_usage_report_in_progress_);
  qp_usage_report_in_progress_ = true;
  // We only want to clear QP samples if an adaptation happened in response to
  // this QP usage. Until we know that one happened (OnAdaptationApplied() is
  // called), we default to false.
  qp_usage_callback_should_clear_qp_samples_ = false;
  // If this triggers adaptation, OnAdaptationApplied() is called, which may
  // modify |qp_usage_callback_should_clear_qp_samples_|.
  OnResourceUsageStateMeasured(ResourceUsageState::kOveruse);
  qp_usage_report_in_progress_ = false;
}

void QualityScalerResource::OnAdaptationApplied(
    const VideoStreamInputState& input_state,
    const VideoSourceRestrictions& restrictions_before,
    const VideoSourceRestrictions& restrictions_after,
    const Resource& reason_resource) {
  // When it comes to feedback to the QualityScaler, we only care about
  // adaptations caused by the QualityScaler.
  if (!qp_usage_report_in_progress_)
    return;
  // An adaptation happened in response to low QP usage. Default to clearing QP
  // samples.
  qp_usage_callback_should_clear_qp_samples_ = true;
  // Under the following cirumstances, if the frame rate before and after
  // adaptation did not differ that much, don't clear the QP samples and instead
  // check for QP again in a short amount of time. This is meant to make
  // QualityScaler quicker at adapting the stream in "balanced" by sometimes
  // rapidly adapting twice.
  // TODO(hbos): Can this be simplified by getting rid of special casing logic?
  // For example, we could decide whether or not to clear QP samples based on
  // how big the adaptation step was alone (regardless of degradation preference
  // or what resource triggered the adaptation) and the QualityScaler could
  // check for QP when it had enough QP samples rather than at a variable
  // interval whose delay is calculated based on events such as these. Now there
  // is much dependency on a specific OnReportQpUsageHigh() event and
  // "balanced", but adaptations happening might not align in a timely manner
  // with the QualityScaler's CheckQpTask.
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
