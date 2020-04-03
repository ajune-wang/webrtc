/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/video_stream_encoder_resource_manager.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/base/macros.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video/video_source_interface.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/video_source_restrictions.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

const int kDefaultInputPixelsWidth = 176;
const int kDefaultInputPixelsHeight = 144;

namespace {

bool IsResolutionScalingEnabled(DegradationPreference degradation_preference) {
  return degradation_preference == DegradationPreference::MAINTAIN_FRAMERATE ||
         degradation_preference == DegradationPreference::BALANCED;
}

bool IsFramerateScalingEnabled(DegradationPreference degradation_preference) {
  return degradation_preference == DegradationPreference::MAINTAIN_RESOLUTION ||
         degradation_preference == DegradationPreference::BALANCED;
}

bool DidIncreaseResolution(VideoSourceRestrictions restrictions_before,
                           VideoSourceRestrictions restrictions_after) {
  if (!restrictions_before.max_pixels_per_frame().has_value()) {
    return false;
  }
  if (!restrictions_after.max_pixels_per_frame().has_value())
    return true;
  return restrictions_after.max_pixels_per_frame().value() >
         restrictions_before.max_pixels_per_frame().value();
}

}  // namespace

class VideoStreamEncoderResourceManager::InitialFrameDropper {
 public:
  explicit InitialFrameDropper(QualityScalerResource* quality_scaler_resource)
      : quality_scaler_resource_(quality_scaler_resource),
        quality_scaler_settings_(QualityScalerSettings::ParseFromFieldTrials()),
        has_seen_first_bwe_drop_(false),
        set_start_bitrate_(DataRate::Zero()),
        set_start_bitrate_time_ms_(0),
        initial_framedrop_(0) {
    RTC_DCHECK(quality_scaler_resource_);
  }

  // Output signal.
  bool DropInitialFrames() const {
    return initial_framedrop_ < kMaxInitialFramedrop;
  }

  // Input signals.
  void SetStartBitrate(DataRate start_bitrate, int64_t now_ms) {
    set_start_bitrate_ = start_bitrate;
    set_start_bitrate_time_ms_ = now_ms;
  }

  void SetTargetBitrate(DataRate target_bitrate, int64_t now_ms) {
    if (set_start_bitrate_ > DataRate::Zero() && !has_seen_first_bwe_drop_ &&
        quality_scaler_resource_->is_started() &&
        quality_scaler_settings_.InitialBitrateIntervalMs() &&
        quality_scaler_settings_.InitialBitrateFactor()) {
      int64_t diff_ms = now_ms - set_start_bitrate_time_ms_;
      if (diff_ms <
              quality_scaler_settings_.InitialBitrateIntervalMs().value() &&
          (target_bitrate <
           (set_start_bitrate_ *
            quality_scaler_settings_.InitialBitrateFactor().value()))) {
        RTC_LOG(LS_INFO) << "Reset initial_framedrop_. Start bitrate: "
                         << set_start_bitrate_.bps()
                         << ", target bitrate: " << target_bitrate.bps();
        initial_framedrop_ = 0;
        has_seen_first_bwe_drop_ = true;
      }
    }
  }

  void OnFrameDroppedDueToSize() { ++initial_framedrop_; }

  void OnMaybeEncodeFrame() { initial_framedrop_ = kMaxInitialFramedrop; }

  void OnQualityScalerSettingsUpdated() {
    if (quality_scaler_resource_->is_started()) {
      // Restart frame drops due to size.
      initial_framedrop_ = 0;
    } else {
      // Quality scaling disabled so we shouldn't drop initial frames.
      initial_framedrop_ = kMaxInitialFramedrop;
    }
  }

 private:
  // The maximum number of frames to drop at beginning of stream to try and
  // achieve desired bitrate.
  static const int kMaxInitialFramedrop = 4;

  const QualityScalerResource* quality_scaler_resource_;
  const QualityScalerSettings quality_scaler_settings_;
  bool has_seen_first_bwe_drop_;
  DataRate set_start_bitrate_;
  int64_t set_start_bitrate_time_ms_;
  // Counts how many frames we've dropped in the initial framedrop phase.
  int initial_framedrop_;
};

VideoStreamEncoderResourceManager::PreventAdaptUpDueToActiveCounts::
    PreventAdaptUpDueToActiveCounts(VideoStreamEncoderResourceManager* manager)
    : manager_(manager) {}

bool VideoStreamEncoderResourceManager::PreventAdaptUpDueToActiveCounts::
    IsAdaptationAllowed(const VideoStreamInputState& input_state,
                        const VideoSourceRestrictions& restrictions_before,
                        const VideoSourceRestrictions& restrictions_after,
                        const Resource* reason_resource) const {
  if (!reason_resource)
    return true;
  AdaptationObserverInterface::AdaptReason reason =
      manager_->ReasonFromResource(*reason_resource);
  // We can't adapt up if we're already at the highest setting.
  // Note that this only includes counts relevant to the current degradation
  // preference. e.g. we previously adapted resolution, now prefer adpating fps,
  // only count the fps adaptations and not the previous resolution adaptations.
  int num_downgrades =
      FilterVideoAdaptationCountersByDegradationPreference(
          manager_->active_counts_[reason],
          manager_->adaptation_processor_->effective_degradation_preference())
          .Total();
  RTC_DCHECK_GE(num_downgrades, 0);
  return num_downgrades > 0;
}

VideoStreamEncoderResourceManager::
    PreventIncreaseResolutionDueToBitrateResource::
        PreventIncreaseResolutionDueToBitrateResource(
            VideoStreamEncoderResourceManager* manager)
    : manager_(manager) {}

bool VideoStreamEncoderResourceManager::
    PreventIncreaseResolutionDueToBitrateResource::IsAdaptationAllowed(
        const VideoStreamInputState& input_state,
        const VideoSourceRestrictions& restrictions_before,
        const VideoSourceRestrictions& restrictions_after,
        const Resource* reason_resource) const {
  if (!reason_resource)
    return true;
  AdaptationObserverInterface::AdaptReason reason =
      manager_->ReasonFromResource(*reason_resource);
  // If increasing resolution due to kQuality, make sure bitrate limits are not
  // violated.
  // TODO(hbos): Why are we allowing violating bitrate constraints if adapting
  // due to CPU?
  if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
      DidIncreaseResolution(restrictions_before, restrictions_after)) {
    uint32_t bitrate_bps = manager_->encoder_target_bitrate_bps_.value_or(0);
    absl::optional<VideoEncoder::ResolutionBitrateLimits> bitrate_limits =
        manager_->encoder_settings_.has_value()
            ? manager_->encoder_settings_->encoder_info()
                  .GetEncoderBitrateLimitsForResolution(
                      // Need some sort of expected resulting pixels to be used
                      // instead of unrestricted.
                      GetHigherResolutionThan(
                          input_state.frame_size_pixels().value()))
            : absl::nullopt;
    if (bitrate_limits.has_value() && bitrate_bps != 0) {
      RTC_DCHECK_GE(bitrate_limits->frame_size_pixels,
                    input_state.frame_size_pixels().value());
      return bitrate_bps >=
             static_cast<uint32_t>(bitrate_limits->min_start_bitrate_bps);
    }
  }
  return true;
}

VideoStreamEncoderResourceManager::PreventAdaptUpInBalancedResource::
    PreventAdaptUpInBalancedResource(VideoStreamEncoderResourceManager* manager)
    : manager_(manager) {}

bool VideoStreamEncoderResourceManager::PreventAdaptUpInBalancedResource::
    IsAdaptationAllowed(const VideoStreamInputState& input_state,
                        const VideoSourceRestrictions& restrictions_before,
                        const VideoSourceRestrictions& restrictions_after,
                        const Resource* reason_resource) const {
  if (!reason_resource)
    return true;
  AdaptationObserverInterface::AdaptReason reason =
      manager_->ReasonFromResource(*reason_resource);
  // Don't adapt if BalancedDegradationSettings applies and determines this will
  // exceed bitrate constraints.
  if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
      manager_->adaptation_processor_->effective_degradation_preference() ==
          DegradationPreference::BALANCED &&
      !manager_->balanced_settings_.CanAdaptUp(
          input_state.video_codec_type(),
          input_state.frame_size_pixels().value(),
          manager_->encoder_target_bitrate_bps_.value_or(0))) {
    return false;
  }
  if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
      DidIncreaseResolution(restrictions_before, restrictions_after) &&
      !manager_->balanced_settings_.CanAdaptUpResolution(
          input_state.video_codec_type(),
          input_state.frame_size_pixels().value(),
          manager_->encoder_target_bitrate_bps_.value_or(0))) {
    return false;
  }
  return true;
}

VideoStreamEncoderResourceManager::VideoStreamEncoderResourceManager(
    ResourceAdaptationProcessor* adaptation_processor,
    VideoStreamInputStateProvider* input_state_provider,
    Clock* clock,
    bool experiment_cpu_load_estimator,
    std::unique_ptr<OveruseFrameDetector> overuse_detector,
    VideoStreamEncoderObserver* encoder_stats_observer,
    ResourceAdaptationProcessorListener* adaptation_listener)
    : prevent_adapt_up_due_to_active_counts_(this),
      prevent_increase_resolution_due_to_bitrate_resource_(this),
      prevent_adapt_up_in_balanced_resource_(this),
      encode_usage_resource_(std::move(overuse_detector)),
      quality_scaler_resource_(adaptation_processor),
      adaptation_processor_(adaptation_processor),
      input_state_provider_(input_state_provider),
      balanced_settings_(),
      source_restrictions_(),
      clock_(clock),
      state_(State::kStopped),
      experiment_cpu_load_estimator_(experiment_cpu_load_estimator),
      initial_frame_dropper_(
          std::make_unique<InitialFrameDropper>(&quality_scaler_resource_)),
      quality_scaling_experiment_enabled_(QualityScalingExperiment::Enabled()),
      encoder_target_bitrate_bps_(absl::nullopt),
      quality_rampup_done_(false),
      quality_rampup_experiment_(QualityRampupExperiment::ParseSettings()),
      encoder_settings_(absl::nullopt),
      encoder_stats_observer_(encoder_stats_observer),
      active_counts_() {
  RTC_DCHECK(encoder_stats_observer_);
  AddResource(&prevent_adapt_up_due_to_active_counts_,
              AdaptationObserverInterface::AdaptReason::kQuality);
  AddResource(&prevent_increase_resolution_due_to_bitrate_resource_,
              AdaptationObserverInterface::AdaptReason::kQuality);
  AddResource(&prevent_adapt_up_in_balanced_resource_,
              AdaptationObserverInterface::AdaptReason::kQuality);
  AddResource(&encode_usage_resource_,
              AdaptationObserverInterface::AdaptReason::kCpu);
  AddResource(&quality_scaler_resource_,
              AdaptationObserverInterface::AdaptReason::kQuality);
}

VideoStreamEncoderResourceManager::~VideoStreamEncoderResourceManager() {
  RTC_DCHECK_EQ(state_, State::kStopped);
}

void VideoStreamEncoderResourceManager::StartResourceAdaptation() {
  RTC_DCHECK_EQ(state_, State::kStopped);
  RTC_DCHECK(encoder_settings_.has_value());
  encode_usage_resource_.StartCheckForOveruse(GetCpuOveruseOptions());
  state_ = State::kStarted;
}

void VideoStreamEncoderResourceManager::StopResourceAdaptation() {
  encode_usage_resource_.StopCheckForOveruse();
  quality_scaler_resource_.StopCheckForOveruse();
  state_ = State::kStopped;
}

void VideoStreamEncoderResourceManager::SetEncoderSettings(
    EncoderSettings encoder_settings) {
  encoder_settings_ = std::move(encoder_settings);

  quality_rampup_experiment_.SetMaxBitrate(
      LastInputFrameSizeOrDefault(),
      encoder_settings_->video_codec().maxBitrate);
  MaybeUpdateTargetFrameRate();
}

void VideoStreamEncoderResourceManager::SetStartBitrate(
    DataRate start_bitrate) {
  if (!start_bitrate.IsZero())
    encoder_target_bitrate_bps_ = start_bitrate.bps();
  initial_frame_dropper_->SetStartBitrate(start_bitrate,
                                          clock_->TimeInMicroseconds());
}

void VideoStreamEncoderResourceManager::SetTargetBitrate(
    DataRate target_bitrate) {
  if (!target_bitrate.IsZero())
    encoder_target_bitrate_bps_ = target_bitrate.bps();
  initial_frame_dropper_->SetTargetBitrate(target_bitrate,
                                           clock_->TimeInMilliseconds());
}

void VideoStreamEncoderResourceManager::SetEncoderRates(
    const VideoEncoder::RateControlParameters& encoder_rates) {
  encoder_rates_ = encoder_rates;
}

void VideoStreamEncoderResourceManager::OnFrameDroppedDueToSize() {
  adaptation_processor_->TriggerAdaptationDueToFrameDroppedDueToSize(
      quality_scaler_resource_);
  initial_frame_dropper_->OnFrameDroppedDueToSize();
}

void VideoStreamEncoderResourceManager::OnEncodeStarted(
    const VideoFrame& cropped_frame,
    int64_t time_when_first_seen_us) {
  encode_usage_resource_.OnEncodeStarted(cropped_frame,
                                         time_when_first_seen_us);
}

void VideoStreamEncoderResourceManager::OnEncodeCompleted(
    const EncodedImage& encoded_image,
    int64_t time_sent_in_us,
    absl::optional<int> encode_duration_us) {
  // Inform |encode_usage_resource_| of the encode completed event.
  uint32_t timestamp = encoded_image.Timestamp();
  int64_t capture_time_us =
      encoded_image.capture_time_ms_ * rtc::kNumMicrosecsPerMillisec;
  encode_usage_resource_.OnEncodeCompleted(timestamp, time_sent_in_us,
                                           capture_time_us, encode_duration_us);
  // Inform |quality_scaler_resource_| of the encode completed event.
  quality_scaler_resource_.OnEncodeCompleted(encoded_image, time_sent_in_us);
}

void VideoStreamEncoderResourceManager::OnFrameDropped(
    EncodedImageCallback::DropReason reason) {
  quality_scaler_resource_.OnFrameDropped(reason);
}

bool VideoStreamEncoderResourceManager::DropInitialFrames() const {
  return initial_frame_dropper_->DropInitialFrames();
}

void VideoStreamEncoderResourceManager::OnMaybeEncodeFrame() {
  initial_frame_dropper_->OnMaybeEncodeFrame();
  MaybePerformQualityRampupExperiment();
}

void VideoStreamEncoderResourceManager::UpdateQualityScalerSettings(
    absl::optional<VideoEncoder::QpThresholds> qp_thresholds) {
  if (qp_thresholds.has_value()) {
    quality_scaler_resource_.StopCheckForOveruse();
    quality_scaler_resource_.StartCheckForOveruse(qp_thresholds.value());
  } else {
    quality_scaler_resource_.StopCheckForOveruse();
  }
  initial_frame_dropper_->OnQualityScalerSettingsUpdated();
}

void VideoStreamEncoderResourceManager::ConfigureQualityScaler(
    const VideoEncoder::EncoderInfo& encoder_info) {
  const auto scaling_settings = encoder_info.scaling_settings;
  const bool quality_scaling_allowed =
      IsResolutionScalingEnabled(
          adaptation_processor_->degradation_preference()) &&
      scaling_settings.thresholds;

  // TODO(https://crbug.com/webrtc/11222): Should this move to
  // QualityScalerResource?
  if (quality_scaling_allowed) {
    if (!quality_scaler_resource_.is_started()) {
      // Quality scaler has not already been configured.

      // Use experimental thresholds if available.
      absl::optional<VideoEncoder::QpThresholds> experimental_thresholds;
      if (quality_scaling_experiment_enabled_) {
        experimental_thresholds = QualityScalingExperiment::GetQpThresholds(
            GetVideoCodecTypeOrGeneric(encoder_settings_));
      }
      UpdateQualityScalerSettings(experimental_thresholds
                                      ? *experimental_thresholds
                                      : *(scaling_settings.thresholds));
    }
  } else {
    UpdateQualityScalerSettings(absl::nullopt);
  }

  // Set the qp-thresholds to the balanced settings if balanced mode.
  if (adaptation_processor_->degradation_preference() ==
          DegradationPreference::BALANCED &&
      quality_scaler_resource_.is_started()) {
    absl::optional<VideoEncoder::QpThresholds> thresholds =
        balanced_settings_.GetQpThresholds(
            GetVideoCodecTypeOrGeneric(encoder_settings_),
            LastInputFrameSizeOrDefault());
    if (thresholds) {
      quality_scaler_resource_.SetQpThresholds(*thresholds);
    }
  }

  encoder_stats_observer_->OnAdaptationChanged(
      VideoStreamEncoderObserver::AdaptationReason::kNone,
      GetActiveCounts(AdaptationObserverInterface::AdaptReason::kCpu),
      GetActiveCounts(AdaptationObserverInterface::AdaptReason::kQuality));
}

// TODO(pbos): Lower these thresholds (to closer to 100%) when we handle
// pipelining encoders better (multiple input frames before something comes
// out). This should effectively turn off CPU adaptations for systems that
// remotely cope with the load right now.
CpuOveruseOptions VideoStreamEncoderResourceManager::GetCpuOveruseOptions()
    const {
  // This is already ensured by the only caller of this method:
  // StartResourceAdaptation().
  RTC_DCHECK(encoder_settings_.has_value());
  CpuOveruseOptions options;
  // Hardware accelerated encoders are assumed to be pipelined; give them
  // additional overuse time.
  if (encoder_settings_->encoder_info().is_hardware_accelerated) {
    options.low_encode_usage_threshold_percent = 150;
    options.high_encode_usage_threshold_percent = 200;
  }
  if (experiment_cpu_load_estimator_) {
    options.filter_time_ms = 5 * rtc::kNumMillisecsPerSec;
  }
  return options;
}

int VideoStreamEncoderResourceManager::LastInputFrameSizeOrDefault() const {
  return input_state_provider_->InputState().frame_size_pixels().value_or(
      kDefaultInputPixelsWidth * kDefaultInputPixelsHeight);
}

void VideoStreamEncoderResourceManager::MaybeUpdateTargetFrameRate() {
  absl::optional<double> codec_max_frame_rate =
      encoder_settings_.has_value()
          ? absl::optional<double>(
                encoder_settings_->video_codec().maxFramerate)
          : absl::nullopt;
  // The current target framerate is the maximum frame rate as specified by
  // the current codec configuration or any limit imposed by the adaptation
  // module. This is used to make sure overuse detection doesn't needlessly
  // trigger in low and/or variable framerate scenarios.
  absl::optional<double> target_frame_rate =
      source_restrictions_.max_frame_rate();
  if (!target_frame_rate.has_value() ||
      (codec_max_frame_rate.has_value() &&
       codec_max_frame_rate.value() < target_frame_rate.value())) {
    target_frame_rate = codec_max_frame_rate;
  }
  encode_usage_resource_.SetTargetFrameRate(target_frame_rate);
}

void VideoStreamEncoderResourceManager::OnVideoSourceRestrictionsUpdated(
    VideoSourceRestrictions restrictions,
    const VideoAdaptationCounters& adaptation_counters,
    const Resource* reason_resource) {
  source_restrictions_ = restrictions;
  if (reason_resource) {
    // A resource signal triggered this adaptation. The adaptation counters have
    // to be updated every time the adaptation counter is incremented or
    // decremented due to a resource.
    AdaptationObserverInterface::AdaptReason reason =
        ReasonFromResource(*reason_resource);
    UpdateAdaptationStats(adaptation_counters, reason);
  } else if (adaptation_counters.Total() == 0) {
    // Adaptation was manually reset - clear the per-reason counters too.
    active_counts_.fill(VideoAdaptationCounters());
  }
  RTC_LOG(LS_INFO) << ActiveCountsToString();
  MaybeUpdateTargetFrameRate();
}

void VideoStreamEncoderResourceManager::AddResource(
    Resource* resource,
    AdaptationObserverInterface::AdaptReason reason) {
  RTC_DCHECK(resource);
  RTC_DCHECK(absl::c_find_if(resources_,
                             [resource](const ResourceAndReason& r) {
                               return r.resource == resource;
                             }) == resources_.end())
      << "Resource " << resource->name() << " already was inserted";
  resources_.emplace_back(resource, reason);
}

std::vector<Resource*> VideoStreamEncoderResourceManager::Resources() const {
  std::vector<Resource*> resources;
  for (const ResourceAndReason& resource_and_reason : resources_)
    resources.push_back(resource_and_reason.resource);
  return resources;
}

AdaptationObserverInterface::AdaptReason
VideoStreamEncoderResourceManager::ReasonFromResource(
    const Resource& resource) const {
  const auto& registered_resource =
      absl::c_find_if(resources_, [&resource](const ResourceAndReason& r) {
        return r.resource == &resource;
      });
  RTC_DCHECK(registered_resource != resources_.end())
      << resource.name() << " not found.";
  return registered_resource->reason;
}

void VideoStreamEncoderResourceManager::OnAdaptationCountChanged(
    const VideoAdaptationCounters& adaptation_count,
    VideoAdaptationCounters* active_count,
    VideoAdaptationCounters* other_active) {
  RTC_DCHECK(active_count);
  RTC_DCHECK(other_active);
  const int active_total = active_count->Total();
  const int other_total = other_active->Total();
  const VideoAdaptationCounters prev_total = *active_count + *other_active;
  const VideoAdaptationCounters delta = adaptation_count - prev_total;

  RTC_DCHECK_EQ(
      std::abs(delta.resolution_adaptations) + std::abs(delta.fps_adaptations),
      1)
      << "Adaptation took more than one step!";

  if (delta.resolution_adaptations > 0) {
    ++active_count->resolution_adaptations;
  } else if (delta.resolution_adaptations < 0) {
    if (active_count->resolution_adaptations == 0) {
      RTC_DCHECK_GT(active_count->fps_adaptations, 0) << "No downgrades left";
      RTC_DCHECK_GT(other_active->resolution_adaptations, 0)
          << "No resolution adaptation to borrow from";
      // Lend an fps adaptation to other and take one resolution adaptation.
      --active_count->fps_adaptations;
      ++other_active->fps_adaptations;
      --other_active->resolution_adaptations;
    } else {
      --active_count->resolution_adaptations;
    }
  }
  if (delta.fps_adaptations > 0) {
    ++active_count->fps_adaptations;
  } else if (delta.fps_adaptations < 0) {
    if (active_count->fps_adaptations == 0) {
      RTC_DCHECK_GT(active_count->resolution_adaptations, 0)
          << "No downgrades left";
      RTC_DCHECK_GT(other_active->fps_adaptations, 0)
          << "No fps adaptation to borrow from";
      // Lend a resolution adaptation to other and take one fps adaptation.
      --active_count->resolution_adaptations;
      ++other_active->resolution_adaptations;
      --other_active->fps_adaptations;
    } else {
      --active_count->fps_adaptations;
    }
  }

  RTC_DCHECK(*active_count + *other_active == adaptation_count);
  RTC_DCHECK_EQ(other_active->Total(), other_total);
  RTC_DCHECK_EQ(active_count->Total(), active_total + delta.Total());
  RTC_DCHECK_GE(active_count->resolution_adaptations, 0);
  RTC_DCHECK_GE(active_count->fps_adaptations, 0);
  RTC_DCHECK_GE(other_active->resolution_adaptations, 0);
  RTC_DCHECK_GE(other_active->fps_adaptations, 0);
}

// TODO(nisse): Delete, once AdaptReason and AdaptationReason are merged.
void VideoStreamEncoderResourceManager::UpdateAdaptationStats(
    const VideoAdaptationCounters& adaptation_counters,
    AdaptationObserverInterface::AdaptReason reason) {
  // Update active counts
  VideoAdaptationCounters& active_count = active_counts_[reason];
  VideoAdaptationCounters& other_active = active_counts_[(reason + 1) % 2];
  OnAdaptationCountChanged(adaptation_counters, &active_count, &other_active);

  switch (reason) {
    case AdaptationObserverInterface::AdaptReason::kCpu:
      encoder_stats_observer_->OnAdaptationChanged(
          VideoStreamEncoderObserver::AdaptationReason::kCpu,
          GetActiveCounts(AdaptationObserverInterface::AdaptReason::kCpu),
          GetActiveCounts(AdaptationObserverInterface::AdaptReason::kQuality));
      break;
    case AdaptationObserverInterface::AdaptReason::kQuality:
      encoder_stats_observer_->OnAdaptationChanged(
          VideoStreamEncoderObserver::AdaptationReason::kQuality,
          GetActiveCounts(AdaptationObserverInterface::AdaptReason::kCpu),
          GetActiveCounts(AdaptationObserverInterface::AdaptReason::kQuality));
      break;
  }
}

VideoStreamEncoderObserver::AdaptationSteps
VideoStreamEncoderResourceManager::GetActiveCounts(
    AdaptationObserverInterface::AdaptReason reason) {
  // TODO(https://crbug.com/webrtc/11392) Ideally this shuold be moved out of
  // this class and into the encoder_stats_observer_.
  const VideoAdaptationCounters counters = active_counts_[reason];

  VideoStreamEncoderObserver::AdaptationSteps counts =
      VideoStreamEncoderObserver::AdaptationSteps();
  counts.num_resolution_reductions = counters.resolution_adaptations;
  counts.num_framerate_reductions = counters.fps_adaptations;
  switch (reason) {
    // TODO(hbos): "Is foo enabled?" sounds like a question best asked of the
    // VideoStreamAdapter, which knows the relevant degradation preference.
    // We do something similar already with
    // VideoStreamAdapter::FilterVideoAdaptationCounters().
    case AdaptationObserverInterface::AdaptReason::kCpu:
      if (!IsFramerateScalingEnabled(
              adaptation_processor_->degradation_preference()))
        counts.num_framerate_reductions = absl::nullopt;
      if (!IsResolutionScalingEnabled(
              adaptation_processor_->degradation_preference()))
        counts.num_resolution_reductions = absl::nullopt;
      break;
    case AdaptationObserverInterface::AdaptReason::kQuality:
      if (!IsFramerateScalingEnabled(
              adaptation_processor_->degradation_preference()) ||
          !quality_scaler_resource_.is_started()) {
        counts.num_framerate_reductions = absl::nullopt;
      }
      if (!IsResolutionScalingEnabled(
              adaptation_processor_->degradation_preference()) ||
          !quality_scaler_resource_.is_started()) {
        counts.num_resolution_reductions = absl::nullopt;
      }
      break;
  }
  return counts;
}

void VideoStreamEncoderResourceManager::MaybePerformQualityRampupExperiment() {
  if (!quality_scaler_resource_.is_started())
    return;

  if (quality_rampup_done_)
    return;

  int64_t now_ms = clock_->TimeInMilliseconds();
  uint32_t bw_kbps = encoder_rates_.has_value()
                         ? encoder_rates_.value().bandwidth_allocation.kbps()
                         : 0;

  bool try_quality_rampup = false;
  if (quality_rampup_experiment_.BwHigh(now_ms, bw_kbps)) {
    // Verify that encoder is at max bitrate and the QP is low.
    if (encoder_settings_ &&
        encoder_target_bitrate_bps_.value_or(0) ==
            encoder_settings_->video_codec().maxBitrate * 1000 &&
        quality_scaler_resource_.QpFastFilterLow()) {
      try_quality_rampup = true;
    }
  }
  // TODO(https://crbug.com/webrtc/11392): See if we can rely on the total
  // counts or the stats, and not the active counts.
  const VideoAdaptationCounters& qp_counts =
      std::get<AdaptationObserverInterface::kQuality>(active_counts_);
  const VideoAdaptationCounters& cpu_counts =
      std::get<AdaptationObserverInterface::kCpu>(active_counts_);
  if (try_quality_rampup && qp_counts.resolution_adaptations > 0 &&
      cpu_counts.Total() == 0) {
    RTC_LOG(LS_INFO) << "Reset quality limitations.";
    adaptation_processor_->ResetVideoSourceRestrictions();
    quality_rampup_done_ = true;
  }
}

std::string VideoStreamEncoderResourceManager::ActiveCountsToString() const {
  rtc::StringBuilder ss;

  ss << "Downgrade counts: fps: {";
  for (size_t reason = 0; reason < active_counts_.size(); ++reason) {
    ss << (reason ? " cpu" : "quality") << ":";
    ss << active_counts_[reason].fps_adaptations;
  }
  ss << "}, resolution {";
  for (size_t reason = 0; reason < active_counts_.size(); ++reason) {
    ss << (reason ? " cpu" : "quality") << ":";
    ss << active_counts_[reason].resolution_adaptations;
  }
  ss << "}";

  return ss.Release();
}

}  // namespace webrtc
