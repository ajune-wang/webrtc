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

#include <utility>

namespace webrtc {

ResourceAdaptationProcessor::ResourceAdaptationProcessor(
    VideoStreamInputStateProvider* input_state_provider,
    ResourceAdaptationProcessorListener* adaptation_listener,
    VideoStreamEncoderObserver* encoder_stats_observer)
    : input_state_provider_(input_state_provider),
      adaptation_listener_(adaptation_listener),
      encoder_stats_observer_(encoder_stats_observer),
      resources_(),
      degradation_preference_(DegradationPreference::DISABLED),
      effective_degradation_preference_(DegradationPreference::DISABLED),
      is_screenshare_(false),
      stream_adapter_(std::make_unique<VideoStreamAdapter>()),
      last_reported_source_restrictions_(),
      processing_in_progress_(false) {}

ResourceAdaptationProcessor::~ResourceAdaptationProcessor() {}

DegradationPreference ResourceAdaptationProcessor::degradation_preference()
    const {
  return degradation_preference_;
}

DegradationPreference
ResourceAdaptationProcessor::effective_degradation_preference() const {
  return effective_degradation_preference_;
}

void ResourceAdaptationProcessor::StartResourceAdaptation() {
  for (auto* resource : resources_) {
    resource->RegisterListener(this);
  }
}

void ResourceAdaptationProcessor::StopResourceAdaptation() {
  for (auto* resource : resources_) {
    resource->UnregisterListener(this);
  }
}

void ResourceAdaptationProcessor::AddResource(Resource* resource) {
  resources_.push_back(resource);
}

void ResourceAdaptationProcessor::SetDegradationPreference(
    DegradationPreference degradation_preference) {
  degradation_preference_ = degradation_preference;
  UpdateEffectiveDegradationPreference();
}

void ResourceAdaptationProcessor::SetIsScreenshare(bool is_screenshare) {
  is_screenshare_ = is_screenshare;
  UpdateEffectiveDegradationPreference();
}

void ResourceAdaptationProcessor::UpdateEffectiveDegradationPreference() {
  effective_degradation_preference_ =
      (is_screenshare_ &&
       degradation_preference_ == DegradationPreference::BALANCED)
          ? DegradationPreference::MAINTAIN_RESOLUTION
          : degradation_preference_;
  stream_adapter_->SetDegradationPreference(effective_degradation_preference_);
  MaybeUpdateVideoSourceRestrictions(nullptr);
}

void ResourceAdaptationProcessor::ResetVideoSourceRestrictions() {
  stream_adapter_->ClearRestrictions();
  MaybeUpdateVideoSourceRestrictions(nullptr);
}

void ResourceAdaptationProcessor::MaybeUpdateVideoSourceRestrictions(
    const Resource* reason) {
  VideoSourceRestrictions new_soure_restrictions =
      FilterRestrictionsByDegradationPreference(
          stream_adapter_->source_restrictions(),
          effective_degradation_preference_);
  if (last_reported_source_restrictions_ != new_soure_restrictions) {
    last_reported_source_restrictions_ = std::move(new_soure_restrictions);
    adaptation_listener_->OnVideoSourceRestrictionsUpdated(
        last_reported_source_restrictions_,
        stream_adapter_->adaptation_counters(), reason);
  }
}

void ResourceAdaptationProcessor::OnResourceUsageStateMeasured(
    const Resource& resource) {
  RTC_DCHECK(resource.usage_state().has_value());
  switch (resource.usage_state().value()) {
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
  return input_state.HasInputFrameSizeAndFramesPerSecond() &&
         (effective_degradation_preference_ !=
              DegradationPreference::MAINTAIN_RESOLUTION ||
          input_state.frames_per_second() >= kMinFrameRateFps);
}

void ResourceAdaptationProcessor::OnResourceUnderuse(
    const Resource& reason_resource) {
  RTC_DCHECK(!processing_in_progress_);
  processing_in_progress_ = true;
  VideoStreamInputState input_state = input_state_provider_->InputState();
  if (effective_degradation_preference_ == DegradationPreference::DISABLED ||
      !HasSufficientInputForAdaptation(input_state)) {
    processing_in_progress_ = false;
    return;
  }
  stream_adapter_->SetInput(input_state);
  // Should we adapt, and if so: how?
  Adaptation adaptation = stream_adapter_->GetAdaptationUp();
  if (adaptation.status() != Adaptation::Status::kValid) {
    processing_in_progress_ = false;
    return;
  }
  // Are all resources OK with this adaptation being applied?
  // TODO(hbos): If rejection can only happen when attempting to adapt up,
  // rename IsAdaptationAllowed to IsAdaptationAllowedUp?
  VideoSourceRestrictions restrictions_before =
      stream_adapter_->source_restrictions();
  VideoSourceRestrictions restrictions_after =
      stream_adapter_->PeekNextRestrictions(adaptation);
  bool can_apply_adaptation = true;
  for (const Resource* resource : resources_) {
    if (!resource->IsAdaptationAllowed(input_state, restrictions_before,
                                       restrictions_after, &reason_resource)) {
      can_apply_adaptation = false;
      break;
    }
  }
  if (!can_apply_adaptation) {
    processing_in_progress_ = false;
    return;
  }
  // Apply adaptation.
  stream_adapter_->ApplyAdaptation(adaptation);
  for (Resource* resource : resources_) {
    resource->ClearUsageState();
  }
  // for (Resource* resource : resources_) {
  //   resource->DidApplyAdaptation(input_state,
  //                                restrictions_before,
  //                                restrictions_after,
  //                                &reason_resource);
  // }
  // Update VideoSourceRestrictions based on adaptation. This also informs the
  // |adaptation_listener_|.
  MaybeUpdateVideoSourceRestrictions(&reason_resource);
  processing_in_progress_ = false;
}

void ResourceAdaptationProcessor::OnResourceOveruse(
    const Resource& reason_resource) {
  RTC_DCHECK(!processing_in_progress_);
  processing_in_progress_ = true;
  VideoStreamInputState input_state = input_state_provider_->InputState();
  if (effective_degradation_preference_ == DegradationPreference::DISABLED ||
      !HasSufficientInputForAdaptation(input_state)) {
    processing_in_progress_ = false;
    return;
  }
  stream_adapter_->SetInput(input_state);
  // Should we adapt, and if so: how?
  Adaptation adaptation = stream_adapter_->GetAdaptationDown();
  if (adaptation.min_pixel_limit_reached())
    encoder_stats_observer_->OnMinPixelLimitReached();
  if (adaptation.status() != Adaptation::Status::kValid) {
    processing_in_progress_ = false;
    return;
  }
  // Apply adaptation.
  VideoSourceRestrictions restrictions_before =
      stream_adapter_->source_restrictions();
  VideoSourceRestrictions restrictions_after =
      stream_adapter_->PeekNextRestrictions(adaptation);
  stream_adapter_->ApplyAdaptation(adaptation);
  for (Resource* resource : resources_) {
    resource->ClearUsageState();
  }
  for (Resource* resource : resources_) {
    resource->DidApplyAdaptation(input_state, restrictions_before,
                                 restrictions_after, &reason_resource);
  }
  // Update VideoSourceRestrictions based on adaptation. This also informs the
  // |adaptation_listener_|.
  MaybeUpdateVideoSourceRestrictions(&reason_resource);
  processing_in_progress_ = false;
}

void ResourceAdaptationProcessor::TriggerAdaptationDueToFrameDroppedDueToSize(
    const Resource& reason_resource) {
  VideoAdaptationCounters counters_before =
      stream_adapter_->adaptation_counters();
  OnResourceOveruse(reason_resource);
  if (degradation_preference_ == DegradationPreference::BALANCED &&
      stream_adapter_->adaptation_counters().fps_adaptations >
          counters_before.fps_adaptations) {
    // Oops, we adapted frame rate. Adapt again, maybe it will adapt resolution!
    OnResourceOveruse(reason_resource);
  }
  if (stream_adapter_->adaptation_counters().resolution_adaptations >
      counters_before.resolution_adaptations) {
    encoder_stats_observer_->OnInitialQualityResolutionAdaptDown();
  }
}

}  // namespace webrtc
