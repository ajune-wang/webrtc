/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/resource_adaptation_processor.h"

#include <algorithm>
#include <utility>

#include "absl/algorithm/container.h"

namespace webrtc {

ResourceAdaptationProcessor::ResourceAdaptationProcessor(
    VideoStreamInputStateProvider* input_state_provider,
    VideoStreamEncoderObserver* encoder_stats_observer)
    : sequence_checker_(),
      is_resource_adaptation_enabled_(false),
      input_state_provider_(input_state_provider),
      encoder_stats_observer_(encoder_stats_observer),
      resources_(),
      degradation_preference_(DegradationPreference::DISABLED),
      effective_degradation_preference_(DegradationPreference::DISABLED),
      is_screenshare_(false),
      stream_adapter_(std::make_unique<VideoStreamAdapter>()),
      last_reported_source_restrictions_(),
      processing_in_progress_(false) {
  sequence_checker_.Detach();
}

ResourceAdaptationProcessor::~ResourceAdaptationProcessor() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(!is_resource_adaptation_enabled_);
  RTC_DCHECK(adaptation_listeners_.empty())
      << "There are listener(s) depending on a ResourceAdaptationProcessor "
      << "being destroyed.";
  RTC_DCHECK(resources_.empty())
      << "There are resource(s) attached to a ResourceAdaptationProcessor "
      << "being destroyed.";
}

void ResourceAdaptationProcessor::InitializeOnResourceAdaptationQueue() {
  // Allows |sequence_checker_| to attach to the resource adaptation queue.
  // The caller is responsible for ensuring that this is the current queue.
  RTC_DCHECK_RUN_ON(&sequence_checker_);
}

DegradationPreference ResourceAdaptationProcessor::degradation_preference()
    const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return degradation_preference_;
}

DegradationPreference
ResourceAdaptationProcessor::effective_degradation_preference() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return effective_degradation_preference_;
}

void ResourceAdaptationProcessor::StartResourceAdaptation() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (is_resource_adaptation_enabled_)
    return;
  for (const auto& resource : resources_) {
    resource->SetResourceListener(this);
  }
  is_resource_adaptation_enabled_ = true;
}

void ResourceAdaptationProcessor::StopResourceAdaptation() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!is_resource_adaptation_enabled_)
    return;
  for (const auto& resource : resources_) {
    resource->SetResourceListener(nullptr);
  }
  is_resource_adaptation_enabled_ = false;
}

void ResourceAdaptationProcessor::AddAdaptationListener(
    ResourceAdaptationProcessorListener* adaptation_listener) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(std::find(adaptation_listeners_.begin(),
                       adaptation_listeners_.end(),
                       adaptation_listener) == adaptation_listeners_.end());
  adaptation_listeners_.push_back(adaptation_listener);
}

void ResourceAdaptationProcessor::RemoveAdaptationListener(
    ResourceAdaptationProcessorListener* adaptation_listener) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  auto it = std::find(adaptation_listeners_.begin(),
                      adaptation_listeners_.end(), adaptation_listener);
  RTC_DCHECK(it != adaptation_listeners_.end());
  adaptation_listeners_.erase(it);
}

void ResourceAdaptationProcessor::AddResource(
    rtc::scoped_refptr<Resource> resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // TODO(hbos): Allow adding resources while |is_resource_adaptation_enabled_|
  // by registering as a listener of the resource on adding it.
  RTC_DCHECK(!is_resource_adaptation_enabled_);
  RTC_DCHECK(std::find(resources_.begin(), resources_.end(), resource) ==
             resources_.end());
  resources_.push_back(resource);
}

void ResourceAdaptationProcessor::RemoveResource(
    rtc::scoped_refptr<Resource> resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // TODO(hbos): Allow removing resources while
  // |is_resource_adaptation_enabled_| by unregistering as a listener of the
  // resource on removing it.
  RTC_DCHECK(!is_resource_adaptation_enabled_);
  auto it = std::find(resources_.begin(), resources_.end(), resource);
  RTC_DCHECK(it != resources_.end());
  resources_.erase(it);
}

void ResourceAdaptationProcessor::SetDegradationPreference(
    DegradationPreference degradation_preference) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  degradation_preference_ = degradation_preference;
  MaybeUpdateEffectiveDegradationPreference();
}

void ResourceAdaptationProcessor::SetIsScreenshare(bool is_screenshare) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  is_screenshare_ = is_screenshare;
  MaybeUpdateEffectiveDegradationPreference();
}

void ResourceAdaptationProcessor::MaybeUpdateEffectiveDegradationPreference() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  effective_degradation_preference_ =
      (is_screenshare_ &&
       degradation_preference_ == DegradationPreference::BALANCED)
          ? DegradationPreference::MAINTAIN_RESOLUTION
          : degradation_preference_;
  stream_adapter_->SetDegradationPreference(effective_degradation_preference_);
  MaybeUpdateVideoSourceRestrictions(nullptr);
}

void ResourceAdaptationProcessor::ResetVideoSourceRestrictions() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  stream_adapter_->ClearRestrictions();
  resource_limited_to_.clear();
  MaybeUpdateVideoSourceRestrictions(nullptr);
}

void ResourceAdaptationProcessor::MaybeUpdateVideoSourceRestrictions(
    rtc::scoped_refptr<Resource> reason) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  VideoSourceRestrictions new_source_restrictions =
      FilterRestrictionsByDegradationPreference(
          stream_adapter_->source_restrictions(),
          effective_degradation_preference_);
  if (last_reported_source_restrictions_ != new_source_restrictions) {
    last_reported_source_restrictions_ = std::move(new_source_restrictions);
    for (auto* adaptation_listener : adaptation_listeners_) {
      adaptation_listener->OnVideoSourceRestrictionsUpdated(
          last_reported_source_restrictions_,
          stream_adapter_->adaptation_counters(), reason);
    }
  }
}

void ResourceAdaptationProcessor::OnResourceUsageStateMeasured(
    rtc::scoped_refptr<Resource> resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(resource->usage_state().has_value());
  switch (resource->usage_state().value()) {
    case ResourceUsageState::kOveruse:
      OnResourceOveruse(resource);
      break;
    case ResourceUsageState::kUnderuse:
      OnResourceUnderuse(resource);
      break;
  }
}

bool ResourceAdaptationProcessor::HasSufficientInputForAdaptation(
    const VideoStreamInputState& input_state) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return input_state.HasInputFrameSizeAndFramesPerSecond() &&
         (effective_degradation_preference_ !=
              DegradationPreference::MAINTAIN_RESOLUTION ||
          input_state.frames_per_second() >= kMinFrameRateFps);
}

void ResourceAdaptationProcessor::OnResourceUnderuse(
    rtc::scoped_refptr<Resource> reason_resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(!processing_in_progress_);
  processing_in_progress_ = true;
  // Clear all usage states. In order to re-run adaptation logic, resources need
  // to provide new resource usage measurements.
  // TODO(hbos): Support not unconditionally clearing usage states by having the
  // ResourceAdaptationProcessor check in on its resources at certain intervals.
  for (const auto& resource : resources_) {
    resource->ClearUsageState();
  }
  VideoStreamInputState input_state = input_state_provider_->InputState();
  if (effective_degradation_preference_ == DegradationPreference::DISABLED ||
      !HasSufficientInputForAdaptation(input_state)) {
    processing_in_progress_ = false;
    return;
  }
  // Update video input states and encoder settings for accurate adaptation.
  stream_adapter_->SetInput(input_state);
  // How can this stream be adapted up?
  Adaptation adaptation = stream_adapter_->GetAdaptationUp();
  if (adaptation.status() != Adaptation::Status::kValid) {
    processing_in_progress_ = false;
    return;
  }
  VideoSourceRestrictions restrictions_before =
      stream_adapter_->source_restrictions();
  VideoStreamAdapter::RestrictionsWithCounters peek_restrictions =
      stream_adapter_->PeekNextRestrictions(adaptation);
  VideoSourceRestrictions restrictions_after = peek_restrictions.restrictions;
  // Check that resource is most limited...
  std::map<const Resource*, VideoAdaptationCounters> most_limited_resources =
      FindMostLimitedResources();
  RTC_DCHECK(!most_limited_resources.empty())
      << "Can not have no limited resources when adaptation status is valid. "
         "Should be kLimitReached.";
  VideoAdaptationCounters most_limited_restrictions =
      most_limited_resources.begin()->second;

  // If the most restricted resource is less limited than current restrictions
  // then proceed with adapting up.
  if (most_limited_restrictions.Total() >=
      stream_adapter_->adaptation_counters().Total()) {
    // If reason_resource is not one of the most limiting resources then abort
    // adaptation.
    if (most_limited_resources.find(reason_resource.get()) ==
        most_limited_resources.end()) {
      processing_in_progress_ = false;
      return;
    }

    UpdateResourceLimitations(reason_resource, peek_restrictions);
    if (most_limited_resources.size() > 1) {
      processing_in_progress_ = false;
      return;
    }
  }
  // Are all resources OK with this adaptation being applied?
  if (!absl::c_all_of(resources_, [&input_state, &restrictions_before,
                                   &restrictions_after, &reason_resource](
                                      rtc::scoped_refptr<Resource> resource) {
        return resource->IsAdaptationUpAllowed(input_state, restrictions_before,
                                               restrictions_after,
                                               reason_resource);
      })) {
    processing_in_progress_ = false;
    return;
  }
  // Apply adaptation.
  stream_adapter_->ApplyAdaptation(adaptation);
  for (const auto& resource : resources_) {
    resource->OnAdaptationApplied(input_state, restrictions_before,
                                  restrictions_after, reason_resource);
  }
  // Update VideoSourceRestrictions based on adaptation. This also informs the
  // |adaptation_listeners_|.
  MaybeUpdateVideoSourceRestrictions(reason_resource);
  processing_in_progress_ = false;
}

void ResourceAdaptationProcessor::OnResourceOveruse(
    rtc::scoped_refptr<Resource> reason_resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(!processing_in_progress_);
  processing_in_progress_ = true;
  // Clear all usage states. In order to re-run adaptation logic, resources need
  // to provide new resource usage measurements.
  // TODO(hbos): Support not unconditionally clearing usage states by having the
  // ResourceAdaptationProcessor check in on its resources at certain intervals.
  for (const auto& resource : resources_) {
    resource->ClearUsageState();
  }
  VideoStreamInputState input_state = input_state_provider_->InputState();
  if (!input_state.has_input()) {
    processing_in_progress_ = false;
    return;
  }
  if (effective_degradation_preference_ == DegradationPreference::DISABLED ||
      !HasSufficientInputForAdaptation(input_state)) {
    processing_in_progress_ = false;
    return;
  }
  // Update video input states and encoder settings for accurate adaptation.
  stream_adapter_->SetInput(input_state);
  // How can this stream be adapted up?
  Adaptation adaptation = stream_adapter_->GetAdaptationDown();
  if (adaptation.min_pixel_limit_reached()) {
    encoder_stats_observer_->OnMinPixelLimitReached();
  }
  if (adaptation.status() != Adaptation::Status::kValid) {
    processing_in_progress_ = false;
    return;
  }
  // Apply adaptation.
  VideoSourceRestrictions restrictions_before =
      stream_adapter_->source_restrictions();
  VideoStreamAdapter::RestrictionsWithCounters peek_next_restrictions =
      stream_adapter_->PeekNextRestrictions(adaptation);
  VideoSourceRestrictions restrictions_after =
      peek_next_restrictions.restrictions;
  UpdateResourceLimitations(reason_resource, peek_next_restrictions);
  stream_adapter_->ApplyAdaptation(adaptation);
  for (const auto& resource : resources_) {
    resource->OnAdaptationApplied(input_state, restrictions_before,
                                  restrictions_after, reason_resource);
  }
  // Update VideoSourceRestrictions based on adaptation. This also informs the
  // |adaptation_listeners_|.
  MaybeUpdateVideoSourceRestrictions(reason_resource);
  processing_in_progress_ = false;
}

void ResourceAdaptationProcessor::TriggerAdaptationDueToFrameDroppedDueToSize(
    rtc::scoped_refptr<Resource> reason_resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  VideoAdaptationCounters counters_before =
      stream_adapter_->adaptation_counters();
  OnResourceOveruse(reason_resource);
  if (degradation_preference_ == DegradationPreference::BALANCED &&
      stream_adapter_->adaptation_counters().fps_adaptations >
          counters_before.fps_adaptations) {
    // Oops, we adapted frame rate. Adapt again, maybe it will adapt resolution!
    // Though this is not guaranteed...
    OnResourceOveruse(reason_resource);
  }
  if (stream_adapter_->adaptation_counters().resolution_adaptations >
      counters_before.resolution_adaptations) {
    encoder_stats_observer_->OnInitialQualityResolutionAdaptDown();
  }
}

std::map<const Resource*, VideoAdaptationCounters>
ResourceAdaptationProcessor::FindMostLimitedResources() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  std::map<const Resource*, VideoAdaptationCounters> most_limited_resources;
  VideoAdaptationCounters most_limited_restrictions;

  for (const auto& resource_restrictions : resource_limited_to_) {
    VideoAdaptationCounters restrictions = resource_restrictions.second;
    if (restrictions.Total() > most_limited_restrictions.Total()) {
      most_limited_restrictions = restrictions;
      most_limited_resources.clear();
      most_limited_resources.insert(resource_restrictions);
    } else if (most_limited_restrictions == restrictions) {
      most_limited_resources.insert(resource_restrictions);
    }
  }
  return most_limited_resources;
}

void ResourceAdaptationProcessor::UpdateResourceLimitations(
    rtc::scoped_refptr<Resource> reason_resource,
    const VideoStreamAdapter::RestrictionsWithCounters&
        peek_next_restrictions) {
  resource_limited_to_[reason_resource.get()] =
      peek_next_restrictions.adaptation_counters;

  for (auto adaptation_listener : adaptation_listeners_) {
    adaptation_listener->OnResourceLimitationChanged(
        reason_resource, peek_next_restrictions.restrictions,
        peek_next_restrictions.adaptation_counters);
  }
}

}  // namespace webrtc
