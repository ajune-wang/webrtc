/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/quality_scaler.h"

#include <math.h>

#include <algorithm>
#include <memory>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/exp_filter.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

// TODO(kthelgason): Some versions of Android have issues with log2.
// See https://code.google.com/p/android/issues/detail?id=212634 for details
#if defined(WEBRTC_ANDROID)
#define log2(x) (log(x) / log(2))
#endif

namespace webrtc {

namespace {
// TODO(nisse): Delete, delegate to encoders.
// Threshold constant used until first downscale (to permit fast rampup).
static const int kMeasureMs = 2000;
static const float kSamplePeriodScaleFactor = 2.5;
static const int kFramedropPercentThreshold = 60;
static const int kMinFramesNeededToScale = 2 * 30;

const char kQualityScalingFieldTrial[] = "WebRTC-Video-QualityScaling";

void GetParamsFromFieldTrialGroup(QualityScaler::Params* params) {
  std::string group =
      webrtc::field_trial::FindFullName(kQualityScalingFieldTrial);
  if (group.empty())
    return;

  int vp8_low, vp8_high, vp9_low, vp9_high, h264_low, h264_high, drop;
  QualityScaler::Params p;
  if (sscanf(group.c_str(), "Enabled-%d,%d,%d,%d,%d,%d,%f,%f,%d", &vp8_low,
             &vp8_high, &vp9_low, &vp9_high, &h264_low, &h264_high,
             &p.alpha_high, &p.alpha_low, &drop) != 9) {
    RTC_LOG(LS_WARNING) << "Invalid number of quality scaler parameters.";
    return;
  }
  p.use_all_drop_reasons = drop > 0;

  if (p.alpha_high < 0 || p.alpha_low < p.alpha_high) {
    RTC_LOG(LS_WARNING) << "Invalid quality scaler parameter value provided.";
    return;
  }
  *params = p;
}
}  // namespace

class QualityScaler::QpSmoother {
 public:
  explicit QpSmoother(float alpha)
      : alpha_(alpha), last_sample_ms_(rtc::TimeMillis()), smoother_(alpha) {}

  rtc::Optional<int> GetAvg() const {
    float value = smoother_.filtered();
    if (value == rtc::ExpFilter::kValueUndefined) {
      return rtc::nullopt;
    }
    return static_cast<int>(value);
  }

  void Add(float sample) {
    int64_t now_ms = rtc::TimeMillis();
    smoother_.Apply(static_cast<float>(now_ms - last_sample_ms_), sample);
    last_sample_ms_ = now_ms;
  }

  void Reset() { smoother_.Reset(alpha_); }

 private:
  const float alpha_;
  int64_t last_sample_ms_;
  rtc::ExpFilter smoother_;
};

class QualityScaler::CheckQPTask : public rtc::QueuedTask {
 public:
  explicit CheckQPTask(QualityScaler* scaler) : scaler_(scaler) {
    RTC_LOG(LS_INFO) << "Created CheckQPTask. Scheduling on queue...";
    rtc::TaskQueue::Current()->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), scaler_->GetSamplingPeriodMs());
  }
  void Stop() {
    RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
    RTC_LOG(LS_INFO) << "Stopping QP Check task.";
    stop_ = true;
  }

 private:
  bool Run() override {
    RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
    if (stop_)
      return true;  // TaskQueue will free this task.
    scaler_->CheckQP();
    rtc::TaskQueue::Current()->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), scaler_->GetSamplingPeriodMs());
    return false;  // Retain the task in order to reuse it.
  }

  QualityScaler* const scaler_;
  bool stop_ = false;
  rtc::SequencedTaskChecker task_checker_;
};

QualityScaler::QualityScaler(AdaptationObserverInterface* observer,
                             VideoEncoder::QpThresholds thresholds)
    : QualityScaler(observer, thresholds, kMeasureMs) {}

// Protected ctor, should not be called directly.
QualityScaler::QualityScaler(AdaptationObserverInterface* observer,
                             VideoEncoder::QpThresholds thresholds,
                             int64_t sampling_period)
    : check_qp_task_(nullptr),
      observer_(observer),
      sampling_period_ms_(sampling_period),
      fast_rampup_(true),
      // Arbitrarily choose size based on 30 fps for 5 seconds.
      average_qp_(5 * 30),
      framedrop_percent_media_opt_(5 * 30),
      framedrop_percent_all_(5 * 30),
      thresholds_(thresholds),
      experiment_enabled_(
          webrtc::field_trial::IsEnabled(kQualityScalingFieldTrial)),
      observed_enough_frames_(false) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  if (experiment_enabled_) {
    GetParamsFromFieldTrialGroup(&params_);
    qp_smoother_high_.reset(new QpSmoother(params_.alpha_high));
    qp_smoother_low_.reset(new QpSmoother(params_.alpha_low));
  }
  RTC_DCHECK(observer_ != nullptr);
  check_qp_task_ = new CheckQPTask(this);
  RTC_LOG(LS_INFO) << "QP thresholds: low: " << thresholds_.low
                   << ", high: " << thresholds_.high;
}

QualityScaler::~QualityScaler() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  check_qp_task_->Stop();
}

int64_t QualityScaler::GetSamplingPeriodMs() const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  if (fast_rampup_) {
    return sampling_period_ms_;
  }
  if (experiment_enabled_ && !observed_enough_frames_) {
    // Use half the interval while waiting for enough frames.
    return sampling_period_ms_ / 2;
  }
  return sampling_period_ms_ * kSamplePeriodScaleFactor;
}

void QualityScaler::ReportDroppedFrameByMediaOpt() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  framedrop_percent_media_opt_.AddSample(100);
  framedrop_percent_all_.AddSample(100);
}

void QualityScaler::ReportDroppedFrameByEncoder() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  framedrop_percent_all_.AddSample(100);
}

void QualityScaler::ReportQP(int qp) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  framedrop_percent_media_opt_.AddSample(0);
  framedrop_percent_all_.AddSample(0);
  average_qp_.AddSample(qp);
  if (qp_smoother_high_)
    qp_smoother_high_->Add(qp);
  if (qp_smoother_low_)
    qp_smoother_low_->Add(qp);
}

void QualityScaler::CheckQP() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  // Should be set through InitEncode -> Should be set by now.
  RTC_DCHECK_GE(thresholds_.low, 0);

  // If we have not observed at least this many frames we can't
  // make a good scaling decision.
  const size_t frames = params_.use_all_drop_reasons
                            ? framedrop_percent_all_.size()
                            : framedrop_percent_media_opt_.size();
  if (frames < kMinFramesNeededToScale) {
    observed_enough_frames_ = false;
    return;
  }
  observed_enough_frames_ = true;

  // Check if we should scale down due to high frame drop.
  const rtc::Optional<int> drop_rate =
      params_.use_all_drop_reasons ? framedrop_percent_all_.GetAverage()
                                   : framedrop_percent_media_opt_.GetAverage();
  if (drop_rate && *drop_rate >= kFramedropPercentThreshold) {
    ReportQPHigh();
    return;
  }

  // Check if we should scale up or down based on QP.
  const rtc::Optional<int> avg_qp_high = qp_smoother_high_
                                             ? qp_smoother_high_->GetAvg()
                                             : average_qp_.GetAverage();
  const rtc::Optional<int> avg_qp_low =
      qp_smoother_low_ ? qp_smoother_low_->GetAvg() : average_qp_.GetAverage();
  if (avg_qp_high && avg_qp_low) {
    RTC_LOG(LS_INFO) << "Checking average QP " << *avg_qp_high << " ("
                     << *avg_qp_low << ").";
    if (*avg_qp_high > thresholds_.high) {
      ReportQPHigh();
      return;
    }
    if (*avg_qp_low <= thresholds_.low) {
      // QP has been low. We want to try a higher resolution.
      ReportQPLow();
      return;
    }
  }
}

void QualityScaler::ReportQPLow() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  ClearSamples();
  observer_->AdaptUp(AdaptationObserverInterface::AdaptReason::kQuality);
}

void QualityScaler::ReportQPHigh() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  ClearSamples();
  observer_->AdaptDown(AdaptationObserverInterface::AdaptReason::kQuality);
  // If we've scaled down, wait longer before scaling up again.
  if (fast_rampup_) {
    fast_rampup_ = false;
  }
}

void QualityScaler::ClearSamples() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  framedrop_percent_media_opt_.Reset();
  framedrop_percent_all_.Reset();
  average_qp_.Reset();
  if (qp_smoother_high_)
    qp_smoother_high_->Reset();
  if (qp_smoother_low_)
    qp_smoother_low_->Reset();
}
}  // namespace webrtc
