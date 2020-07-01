/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/video_stream_adapter.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/video/video_adaptation_counters.h"
#include "api/video/video_adaptation_reason.h"
#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/synchronization/sequence_checker.h"

namespace webrtc {

const int kMinFrameRateFps = 2;

namespace {

// Generate suggested higher and lower frame rates and resolutions, to be
// applied to the VideoSourceRestrictor. These are used in "maintain-resolution"
// and "maintain-framerate". The "balanced" degradation preference also makes
// use of BalancedDegradationPreference when generating suggestions. The
// VideoSourceRestrictor decidedes whether or not a proposed adaptation is
// valid.

// For frame rate, the steps we take are 2/3 (down) and 3/2 (up).
int GetLowerFrameRateThan(int fps) {
  RTC_DCHECK(fps != std::numeric_limits<int>::max());
  return (fps * 2) / 3;
}
// TODO(hbos): Use absl::optional<> instead?
int GetHigherFrameRateThan(int fps) {
  return fps != std::numeric_limits<int>::max()
             ? (fps * 3) / 2
             : std::numeric_limits<int>::max();
}

// For resolution, the steps we take are 3/5 (down) and 5/3 (up).
// Notice the asymmetry of which restriction property is set depending on if
// we are adapting up or down:
// - VideoSourceRestrictor::DecreaseResolution() sets the max_pixels_per_frame()
//   to the desired target and target_pixels_per_frame() to null.
// - VideoSourceRestrictor::IncreaseResolutionTo() sets the
//   target_pixels_per_frame() to the desired target, and max_pixels_per_frame()
//   is set according to VideoSourceRestrictor::GetIncreasedMaxPixelsWanted().
int GetLowerResolutionThan(int pixel_count) {
  RTC_DCHECK(pixel_count != std::numeric_limits<int>::max());
  return (pixel_count * 3) / 5;
}

}  // namespace

VideoSourceRestrictionsListener::~VideoSourceRestrictionsListener() = default;

VideoSourceRestrictions FilterRestrictionsByDegradationPreference(
    VideoSourceRestrictions source_restrictions,
    DegradationPreference degradation_preference) {
  switch (degradation_preference) {
    case DegradationPreference::BALANCED:
      break;
    case DegradationPreference::MAINTAIN_FRAMERATE:
      source_restrictions.set_max_frame_rate(absl::nullopt);
      break;
    case DegradationPreference::MAINTAIN_RESOLUTION:
      source_restrictions.set_max_pixels_per_frame(absl::nullopt);
      source_restrictions.set_target_pixels_per_frame(absl::nullopt);
      break;
    case DegradationPreference::DISABLED:
      source_restrictions.set_max_pixels_per_frame(absl::nullopt);
      source_restrictions.set_target_pixels_per_frame(absl::nullopt);
      source_restrictions.set_max_frame_rate(absl::nullopt);
  }
  return source_restrictions;
}

// TODO(hbos): Use absl::optional<> instead?
int GetHigherResolutionThan(int pixel_count) {
  return pixel_count != std::numeric_limits<int>::max()
             ? (pixel_count * 5) / 3
             : std::numeric_limits<int>::max();
}

// static
const char* Adaptation::StatusToString(Adaptation::Status status) {
  switch (status) {
    case Adaptation::Status::kValid:
      return "kValid";
    case Adaptation::Status::kLimitReached:
      return "kLimitReached";
    case Adaptation::Status::kAwaitingPreviousAdaptation:
      return "kAwaitingPreviousAdaptation";
    case Status::kInsufficientInput:
      return "kInsufficientInput";
  }
}

Adaptation::Adaptation(int validation_id,
                       VideoSourceRestrictions restrictions,
                       VideoAdaptationCounters counters,
                       VideoStreamInputState input_state,
                       bool min_pixel_limit_reached)
    : validation_id_(validation_id),
      status_(Status::kValid),
      min_pixel_limit_reached_(min_pixel_limit_reached),
      restrictions_(std::move(restrictions)),
      counters_(std::move(counters)),
      input_state_(std::move(input_state)) {}

Adaptation::Adaptation(int validation_id,
                       Status invalid_status,
                       VideoStreamInputState input_state,
                       bool min_pixel_limit_reached)
    : validation_id_(validation_id),
      status_(invalid_status),
      min_pixel_limit_reached_(min_pixel_limit_reached),
      input_state_(std::move(input_state)) {
  RTC_DCHECK_NE(status_, Status::kValid);
}

Adaptation::Status Adaptation::status() const {
  return status_;
}

bool Adaptation::min_pixel_limit_reached() const {
  return min_pixel_limit_reached_;
}

const VideoStreamInputState& Adaptation::input_state() const {
  return input_state_;
}

const VideoSourceRestrictions& Adaptation::restrictions() const {
  return restrictions_;
}

const VideoAdaptationCounters& Adaptation::counters() const {
  return counters_;
}

// VideoSourceRestrictor is responsible for keeping track of current
// VideoSourceRestrictions.
class VideoStreamAdapter::VideoSourceRestrictor {
 public:
  VideoSourceRestrictor() {}

  void set_min_pixels_per_frame(int min_pixels_per_frame) {
    min_pixels_per_frame_ = min_pixels_per_frame;
  }

  int min_pixels_per_frame() const { return min_pixels_per_frame_; }

  bool CanDecreaseResolutionTo(
      int target_pixels,
      const VideoSourceRestrictions& restrictions) const {
    int max_pixels_per_frame =
        rtc::dchecked_cast<int>(restrictions.max_pixels_per_frame().value_or(
            std::numeric_limits<int>::max()));
    return target_pixels < max_pixels_per_frame &&
           target_pixels >= min_pixels_per_frame_;
  }

  bool CanIncreaseResolutionTo(
      int target_pixels,
      const VideoSourceRestrictions& restrictions) const {
    int max_pixels_wanted = GetIncreasedMaxPixelsWanted(target_pixels);
    int max_pixels_per_frame =
        rtc::dchecked_cast<int>(restrictions.max_pixels_per_frame().value_or(
            std::numeric_limits<int>::max()));
    return max_pixels_wanted > max_pixels_per_frame;
  }

  bool CanDecreaseFrameRateTo(
      int max_frame_rate,
      const VideoSourceRestrictions& restrictions) const {
    const int fps_wanted = std::max(kMinFrameRateFps, max_frame_rate);
    return fps_wanted <
           rtc::dchecked_cast<int>(restrictions.max_frame_rate().value_or(
               std::numeric_limits<int>::max()));
  }

  bool CanIncreaseFrameRateTo(
      int max_frame_rate,
      const VideoSourceRestrictions& restrictions) const {
    return max_frame_rate >
           rtc::dchecked_cast<int>(restrictions.max_frame_rate().value_or(
               std::numeric_limits<int>::max()));
  }

  VideoStreamAdapter::RestrictionsWithCounters DecreaseResolutionTo(
      int target_pixels,
      const VideoSourceRestrictions& current_restrictions,
      const VideoAdaptationCounters& current_adaptations) {
    RTC_DCHECK(CanDecreaseResolutionTo(target_pixels, current_restrictions));
    VideoSourceRestrictions restrictions = current_restrictions;
    VideoAdaptationCounters adaptations = current_adaptations;
    RTC_LOG(LS_INFO) << "Scaling down resolution, max pixels: "
                     << target_pixels;
    restrictions.set_max_pixels_per_frame(
        target_pixels != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(target_pixels)
            : absl::nullopt);
    restrictions.set_target_pixels_per_frame(absl::nullopt);
    ++adaptations.resolution_adaptations;
    return {restrictions, adaptations};
  }

  VideoStreamAdapter::RestrictionsWithCounters IncreaseResolutionTo(
      int target_pixels,
      const VideoSourceRestrictions& current_restrictions,
      const VideoAdaptationCounters& current_adaptations) {
    RTC_DCHECK(CanIncreaseResolutionTo(target_pixels, current_restrictions));
    int max_pixels_wanted = GetIncreasedMaxPixelsWanted(target_pixels);
    VideoSourceRestrictions restrictions = current_restrictions;
    VideoAdaptationCounters adaptations = current_adaptations;
    RTC_LOG(LS_INFO) << "Scaling up resolution, max pixels: "
                     << max_pixels_wanted;
    restrictions.set_max_pixels_per_frame(
        max_pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(max_pixels_wanted)
            : absl::nullopt);
    restrictions.set_target_pixels_per_frame(
        max_pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(target_pixels)
            : absl::nullopt);
    --adaptations.resolution_adaptations;
    RTC_DCHECK_GE(adaptations.resolution_adaptations, 0);
    return {restrictions, adaptations};
  }

  VideoStreamAdapter::RestrictionsWithCounters DecreaseFrameRateTo(
      int max_frame_rate,
      const VideoSourceRestrictions& current_restrictions,
      const VideoAdaptationCounters& current_adaptations) {
    RTC_DCHECK(CanDecreaseFrameRateTo(max_frame_rate, current_restrictions));
    max_frame_rate = std::max(kMinFrameRateFps, max_frame_rate);
    RTC_LOG(LS_INFO) << "Scaling down framerate: " << max_frame_rate;
    VideoSourceRestrictions restrictions = current_restrictions;
    VideoAdaptationCounters adaptations = current_adaptations;
    restrictions.set_max_frame_rate(max_frame_rate !=
                                            std::numeric_limits<int>::max()
                                        ? absl::optional<double>(max_frame_rate)
                                        : absl::nullopt);
    ++adaptations.fps_adaptations;
    return {restrictions, adaptations};
  }

  VideoStreamAdapter::RestrictionsWithCounters IncreaseFrameRateTo(
      int max_frame_rate,
      bool is_balanced,
      const VideoSourceRestrictions& current_restrictions,
      const VideoAdaptationCounters& current_adaptations) {
    RTC_DCHECK(CanIncreaseFrameRateTo(max_frame_rate, current_restrictions));
    RTC_LOG(LS_INFO) << "Scaling up framerate: " << max_frame_rate;
    VideoSourceRestrictions restrictions = current_restrictions;
    VideoAdaptationCounters adaptations = current_adaptations;

    restrictions.set_max_frame_rate(max_frame_rate !=
                                            std::numeric_limits<int>::max()
                                        ? absl::optional<double>(max_frame_rate)
                                        : absl::nullopt);
    --adaptations.fps_adaptations;
    RTC_DCHECK_GE(adaptations.fps_adaptations, 0);
    if (is_balanced && adaptations.fps_adaptations == 0 &&
        max_frame_rate != std::numeric_limits<int>::max()) {
      RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
      restrictions.set_max_frame_rate(absl::nullopt);
    }

    return {restrictions, adaptations};
  }

 private:
  static int GetIncreasedMaxPixelsWanted(int target_pixels) {
    if (target_pixels == std::numeric_limits<int>::max())
      return std::numeric_limits<int>::max();
    // When we decrease resolution, we go down to at most 3/5 of current pixels.
    // Thus to increase resolution, we need 3/5 to get back to where we started.
    // When going up, the desired max_pixels_per_frame() has to be significantly
    // higher than the target because the source's native resolutions might not
    // match the target. We pick 12/5 of the target.
    //
    // (This value was historically 4 times the old target, which is (3/5)*4 of
    // the new target - or 12/5 - assuming the target is adjusted according to
    // the above steps.)
    RTC_DCHECK(target_pixels != std::numeric_limits<int>::max());
    return (target_pixels * 12) / 5;
  }

  // Needed by CanDecreaseResolutionTo().
  int min_pixels_per_frame_ = 0;
};

VideoStreamAdapter::VideoStreamAdapter(
    VideoStreamInputStateProvider* input_state_provider)
    : source_restrictor_(std::make_unique<VideoSourceRestrictor>()),
      input_state_provider_(input_state_provider),
      balanced_settings_(),
      adaptation_validation_id_(0),
      degradation_preference_(DegradationPreference::DISABLED),
      awaiting_frame_size_change_(absl::nullopt),
      last_video_source_restrictions_() {
  sequence_checker_.Detach();
}

VideoStreamAdapter::~VideoStreamAdapter() {}

VideoSourceRestrictions VideoStreamAdapter::source_restrictions() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return restrictions_;
}

const VideoAdaptationCounters& VideoStreamAdapter::adaptation_counters() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return counters_;
}

void VideoStreamAdapter::ClearRestrictions() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // Invalidate any previously returned Adaptation.
  RTC_LOG(INFO) << "Resetting restrictions";
  ++adaptation_validation_id_;
  restrictions_ = VideoSourceRestrictions();
  counters_ = VideoAdaptationCounters();
  awaiting_frame_size_change_ = absl::nullopt;
  BroadcastVideoRestrictionsUpdate(nullptr);
}

void VideoStreamAdapter::AddRestrictionsListener(
    VideoSourceRestrictionsListener* restrictions_listener) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(std::find(restrictions_listeners_.begin(),
                       restrictions_listeners_.end(),
                       restrictions_listener) == restrictions_listeners_.end());
  restrictions_listeners_.push_back(restrictions_listener);
}

void VideoStreamAdapter::RemoveRestrictionsListener(
    VideoSourceRestrictionsListener* restrictions_listener) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  auto it = std::find(restrictions_listeners_.begin(),
                      restrictions_listeners_.end(), restrictions_listener);
  RTC_DCHECK(it != restrictions_listeners_.end());
  restrictions_listeners_.erase(it);
}

void VideoStreamAdapter::SetDegradationPreference(
    DegradationPreference degradation_preference) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (degradation_preference_ == degradation_preference)
    return;
  // Invalidate any previously returned Adaptation.
  ++adaptation_validation_id_;
  bool balanced_switch =
      degradation_preference == DegradationPreference::BALANCED ||
      degradation_preference_ == DegradationPreference::BALANCED;
  degradation_preference_ = degradation_preference;
  if (balanced_switch) {
    // ClearRestrictions() calls BroadcastVideoRestrictionsUpdate(nullptr).
    ClearRestrictions();
  } else {
    BroadcastVideoRestrictionsUpdate(nullptr);
  }
}

struct VideoStreamAdapter::RestrictionsOrStateVisitor {
  Adaptation operator()(const RestrictionsWithCounters& r) const {
    RTC_DCHECK_RUN_ON(&parent->sequence_checker_);
    return Adaptation(parent->adaptation_validation_id_, r.first, r.second,
                      input_state, min_pixel_limit_reached());
  }
  Adaptation operator()(const Adaptation::Status& status) const {
    RTC_DCHECK_NE(status, Adaptation::Status::kValid);
    RTC_DCHECK_RUN_ON(&parent->sequence_checker_);
    return Adaptation(parent->adaptation_validation_id_, status, input_state,
                      min_pixel_limit_reached());
  }
  bool min_pixel_limit_reached() const RTC_RUN_ON(&parent->sequence_checker_) {
    return input_state.frame_size_pixels().has_value() &&
           GetLowerFrameRateThan(input_state.frame_size_pixels().value()) <
               parent->source_restrictor_->min_pixels_per_frame();
  }

  const VideoStreamAdapter* parent;
  const VideoStreamInputState& input_state;
};

Adaptation VideoStreamAdapter::RestrictionsOrStateToAdaptation(
    VideoStreamAdapter::RestrictionsOrState step_or_state,
    const VideoStreamInputState& input_state) const {
  RTC_DCHECK(!step_or_state.valueless_by_exception());
  return absl::visit(RestrictionsOrStateVisitor{this, input_state},
                     step_or_state);
}

Adaptation VideoStreamAdapter::GetAdaptationUp(
    const VideoStreamInputState& input_state) const {
  return RestrictionsOrStateToAdaptation(GetAdaptationUpStep(input_state),
                                         input_state);
}

Adaptation VideoStreamAdapter::GetAdaptationUp() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK_NE(degradation_preference_, DegradationPreference::DISABLED);
  VideoStreamInputState input_state = input_state_provider_->InputState();
  ++adaptation_validation_id_;
  Adaptation adaptation = GetAdaptationUp(input_state);
  return adaptation;
}

VideoStreamAdapter::RestrictionsOrState VideoStreamAdapter::GetAdaptationUpStep(
    const VideoStreamInputState& input_state) const {
  if (!HasSufficientInputForAdaptation(input_state)) {
    return Adaptation::Status::kInsufficientInput;
  }
  source_restrictor_->set_min_pixels_per_frame(
      input_state.min_pixels_per_frame());
  // Don't adapt if we're awaiting a previous adaptation to have an effect.
  if (awaiting_frame_size_change_ &&
      awaiting_frame_size_change_->pixels_increased &&
      degradation_preference_ == DegradationPreference::MAINTAIN_FRAMERATE &&
      input_state.frame_size_pixels().value() <=
          awaiting_frame_size_change_->frame_size_pixels) {
    return Adaptation::Status::kAwaitingPreviousAdaptation;
  }

  // Maybe propose targets based on degradation preference.
  switch (degradation_preference_) {
    case DegradationPreference::BALANCED: {
      // Attempt to increase target frame rate.
      int target_fps =
          balanced_settings_.MaxFps(input_state.video_codec_type(),
                                    input_state.frame_size_pixels().value());
      if (source_restrictor_->CanIncreaseFrameRateTo(target_fps,
                                                     restrictions_)) {
        return source_restrictor_->IncreaseFrameRateTo(
            target_fps, true, restrictions_, counters_);
      }
      // Scale up resolution.
      ABSL_FALLTHROUGH_INTENDED;
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Attempt to increase pixel count.
      int target_pixels = input_state.frame_size_pixels().value();
      if (counters_.resolution_adaptations == 1) {
        RTC_LOG(LS_INFO) << "Removing resolution down-scaling setting.";
        target_pixels = std::numeric_limits<int>::max();
      }
      target_pixels = GetHigherResolutionThan(target_pixels);
      if (!source_restrictor_->CanIncreaseResolutionTo(target_pixels,
                                                       restrictions_)) {
        return Adaptation::Status::kLimitReached;
      }
      return source_restrictor_->IncreaseResolutionTo(target_pixels,
                                                      restrictions_, counters_);
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      // Scale up framerate.
      int target_fps = input_state.frames_per_second();
      if (counters_.fps_adaptations == 1) {
        RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
        target_fps = std::numeric_limits<int>::max();
      }
      target_fps = GetHigherFrameRateThan(target_fps);
      if (!source_restrictor_->CanIncreaseFrameRateTo(target_fps,
                                                      restrictions_)) {
        return Adaptation::Status::kLimitReached;
      }
      return source_restrictor_->IncreaseFrameRateTo(target_fps, false,
                                                     restrictions_, counters_);
    }
    case DegradationPreference::DISABLED:
      RTC_NOTREACHED();
      return Adaptation::Status::kLimitReached;
  }
}

Adaptation VideoStreamAdapter::GetAdaptationDown() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK_NE(degradation_preference_, DegradationPreference::DISABLED);
  VideoStreamInputState input_state = input_state_provider_->InputState();
  ++adaptation_validation_id_;
  return RestrictionsOrStateToAdaptation(GetAdaptationDownStep(input_state),
                                         input_state);
}

VideoStreamAdapter::RestrictionsOrState
VideoStreamAdapter::GetAdaptationDownStep(
    const VideoStreamInputState& input_state) const {
  if (!HasSufficientInputForAdaptation(input_state)) {
    return Adaptation::Status::kInsufficientInput;
  }
  source_restrictor_->set_min_pixels_per_frame(
      input_state.min_pixels_per_frame());
  // Don't adapt if we're awaiting a previous adaptation to have an effect or
  // if we switched degradation preference.
  if (awaiting_frame_size_change_ &&
      !awaiting_frame_size_change_->pixels_increased &&
      degradation_preference_ == DegradationPreference::MAINTAIN_FRAMERATE &&
      input_state.frame_size_pixels().value() >=
          awaiting_frame_size_change_->frame_size_pixels) {
    return Adaptation::Status::kAwaitingPreviousAdaptation;
  }
  // Maybe propose targets based on degradation preference.
  switch (degradation_preference_) {
    case DegradationPreference::BALANCED: {
      // Try scale down framerate, if lower.
      int target_fps =
          balanced_settings_.MinFps(input_state.video_codec_type(),
                                    input_state.frame_size_pixels().value());
      if (source_restrictor_->CanDecreaseFrameRateTo(target_fps,
                                                     restrictions_)) {
        return source_restrictor_->DecreaseFrameRateTo(
            target_fps, restrictions_, counters_);
      }
      // Scale down resolution.
      ABSL_FALLTHROUGH_INTENDED;
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Scale down resolution.
      int target_pixels =
          GetLowerResolutionThan(input_state.frame_size_pixels().value());
      if (!source_restrictor_->CanDecreaseResolutionTo(target_pixels,
                                                       restrictions_)) {
        return Adaptation::Status::kLimitReached;
      }
      return source_restrictor_->DecreaseResolutionTo(target_pixels,
                                                      restrictions_, counters_);
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      int target_fps = GetLowerFrameRateThan(input_state.frames_per_second());
      if (!source_restrictor_->CanDecreaseFrameRateTo(target_fps,
                                                      restrictions_)) {
        return Adaptation::Status::kLimitReached;
      }
      return source_restrictor_->DecreaseFrameRateTo(target_fps, restrictions_,
                                                     counters_);
    }
    case DegradationPreference::DISABLED:
      RTC_NOTREACHED();
      return Adaptation::Status::kLimitReached;
  }
}

Adaptation VideoStreamAdapter::GetAdaptDownResolution() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  VideoStreamInputState input_state = input_state_provider_->InputState();
  switch (degradation_preference_) {
    case DegradationPreference::DISABLED:
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      return Adaptation(adaptation_validation_id_,
                        Adaptation::Status::kLimitReached, input_state, false);
    }
    case DegradationPreference::MAINTAIN_FRAMERATE:
      return GetAdaptationDown();
    case DegradationPreference::BALANCED: {
      // Adapt twice if the first adaptation did not decrease resolution.
      Adaptation first_adaptation = RestrictionsOrStateToAdaptation(
          GetAdaptationDownStep(input_state), input_state);
      if (first_adaptation.status() != Adaptation::Status::kValid) {
        return first_adaptation;
      }
      if (first_adaptation.counters().resolution_adaptations >
          counters_.resolution_adaptations) {
        return first_adaptation;
      }
      // We didn't decrease resolution so force it; amend a resolution resuction
      // to the existing framerate reduction in |first_adaptation|.
      int target_pixels =
          GetLowerResolutionThan(input_state.frame_size_pixels().value());
      if (!source_restrictor_->CanDecreaseResolutionTo(
              target_pixels, first_adaptation.restrictions())) {
        return first_adaptation;
      }
      return RestrictionsOrStateToAdaptation(
          source_restrictor_->DecreaseResolutionTo(
              target_pixels, first_adaptation.restrictions(),
              first_adaptation.counters()),
          input_state);
    }
    default:
      RTC_NOTREACHED();
  }
}

void VideoStreamAdapter::ApplyAdaptation(
    const Adaptation& adaptation,
    rtc::scoped_refptr<Resource> resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK_EQ(adaptation.validation_id_, adaptation_validation_id_);
  if (adaptation.status() != Adaptation::Status::kValid)
    return;
  // Remember the input pixels and fps of this adaptation. Used to avoid
  // adapting again before this adaptation has had an effect.
  if (DidIncreaseResolution(restrictions_, adaptation.restrictions())) {
    awaiting_frame_size_change_.emplace(
        true, adaptation.input_state().frame_size_pixels().value());
  } else if (DidDecreaseResolution(restrictions_, adaptation.restrictions())) {
    awaiting_frame_size_change_.emplace(
        false, adaptation.input_state().frame_size_pixels().value());
  } else {
    awaiting_frame_size_change_ = absl::nullopt;
  }
  // Adapt!
  restrictions_ = adaptation.restrictions();
  counters_ = adaptation.counters();
  BroadcastVideoRestrictionsUpdate(resource);
}

Adaptation VideoStreamAdapter::GetAdaptationTo(
    const VideoAdaptationCounters& counters,
    const VideoSourceRestrictions& restrictions) {
  // Adapts up/down from the current levels so counters are equal.
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  VideoStreamInputState input_state = input_state_provider_->InputState();
  return Adaptation(adaptation_validation_id_, restrictions, counters,
                    input_state, false);
}

void VideoStreamAdapter::BroadcastVideoRestrictionsUpdate(
    const rtc::scoped_refptr<Resource>& resource) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  VideoSourceRestrictions filtered = FilterRestrictionsByDegradationPreference(
      source_restrictions(), degradation_preference_);
  if (last_filtered_restrictions_ == filtered) {
    return;
  }
  for (auto* restrictions_listener : restrictions_listeners_) {
    restrictions_listener->OnVideoSourceRestrictionsUpdated(
        filtered, counters_, resource, source_restrictions());
  }
  last_video_source_restrictions_ = restrictions_;
  last_filtered_restrictions_ = filtered;
}

bool VideoStreamAdapter::HasSufficientInputForAdaptation(
    const VideoStreamInputState& input_state) const {
  return input_state.HasInputFrameSizeAndFramesPerSecond() &&
         (degradation_preference_ !=
              DegradationPreference::MAINTAIN_RESOLUTION ||
          input_state.frames_per_second() >= kMinFrameRateFps);
}

VideoStreamAdapter::AwaitingFrameSizeChange::AwaitingFrameSizeChange(
    bool pixels_increased,
    int frame_size_pixels)
    : pixels_increased(pixels_increased),
      frame_size_pixels(frame_size_pixels) {}

}  // namespace webrtc
