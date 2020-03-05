/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/video_stream_adaptor.h"

#include <algorithm>
#include <limits>

#include "absl/types/optional.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

const int VideoStreamAdaptor::kMinFramerateFps = 2;

VideoStreamAdaptor::AdaptationTarget::AdaptationTarget(AdaptationAction action,
                                                       int value)
    : action(action), value(value) {}

// VideoSourceRestrictor is responsible for keeping track of current
// VideoSourceRestrictions and how to modify them in response to adapting up or
// down. It is not reponsible for determining when we should adapt up or down.
class VideoStreamAdaptor::VideoSourceRestrictor {
 public:
  // For frame rate, the steps we take are 2/3 (down) and 3/2 (up).
  static int GetLowerFrameRateThan(int fps) {
    RTC_DCHECK(fps != std::numeric_limits<int>::max());
    return (fps * 2) / 3;
  }
  // TODO(hbos): Use absl::optional<> instead?
  static int GetHigherFrameRateThan(int fps) {
    return fps != std::numeric_limits<int>::max()
               ? (fps * 3) / 2
               : std::numeric_limits<int>::max();
  }

  // For resolution, the steps we take are 3/5 (down) and 5/3 (up).
  // Notice the asymmetry of which restriction property is set depending on if
  // we are adapting up or down:
  // - DecreaseResolution() sets the max_pixels_per_frame() to the desired
  //   target and target_pixels_per_frame() to null.
  // - IncreaseResolutionTo() sets the target_pixels_per_frame() to the desired
  //   target, and max_pixels_per_frame() is set according to
  //   GetIncreasedMaxPixelsWanted().
  static int GetLowerResolutionThan(int pixel_count) {
    RTC_DCHECK(pixel_count != std::numeric_limits<int>::max());
    return (pixel_count * 3) / 5;
  }
  // TODO(hbos): Use absl::optional<> instead?
  static int GetHigherResolutionThan(int pixel_count) {
    return pixel_count != std::numeric_limits<int>::max()
               ? (pixel_count * 5) / 3
               : std::numeric_limits<int>::max();
  }

  VideoSourceRestrictor() {}

  VideoSourceRestrictions source_restrictions() { return source_restrictions_; }
  const AdaptationCounters& adaptation_counters() const { return adaptations_; }
  void ClearRestrictions() {
    source_restrictions_ = VideoSourceRestrictions();
    adaptations_ = AdaptationCounters();
  }

  bool CanDecreaseResolutionTo(int target_pixels, int min_pixels_per_frame) {
    int max_pixels_per_frame = rtc::dchecked_cast<int>(
        source_restrictions_.max_pixels_per_frame().value_or(
            std::numeric_limits<int>::max()));
    return target_pixels < max_pixels_per_frame &&
           target_pixels >= min_pixels_per_frame;
  }
  void DecreaseResolutionTo(int target_pixels, int min_pixels_per_frame) {
    RTC_DCHECK(CanDecreaseResolutionTo(target_pixels, min_pixels_per_frame));
    RTC_LOG(LS_INFO) << "Scaling down resolution, max pixels: "
                     << target_pixels;
    source_restrictions_.set_max_pixels_per_frame(
        target_pixels != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(target_pixels)
            : absl::nullopt);
    source_restrictions_.set_target_pixels_per_frame(absl::nullopt);
    ++adaptations_.resolutions_adaptations;
  }

  bool CanIncreaseResolutionTo(int target_pixels) {
    int max_pixels_wanted = GetIncreasedMaxPixelsWanted(target_pixels);
    int max_pixels_per_frame = rtc::dchecked_cast<int>(
        source_restrictions_.max_pixels_per_frame().value_or(
            std::numeric_limits<int>::max()));
    return max_pixels_wanted > max_pixels_per_frame;
  }
  void IncreaseResolutionTo(int target_pixels) {
    RTC_DCHECK(CanIncreaseResolutionTo(target_pixels));
    int max_pixels_wanted = GetIncreasedMaxPixelsWanted(target_pixels);
    RTC_LOG(LS_INFO) << "Scaling up resolution, max pixels: "
                     << max_pixels_wanted;
    source_restrictions_.set_max_pixels_per_frame(
        max_pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(max_pixels_wanted)
            : absl::nullopt);
    source_restrictions_.set_target_pixels_per_frame(
        max_pixels_wanted != std::numeric_limits<int>::max()
            ? absl::optional<size_t>(target_pixels)
            : absl::nullopt);
    --adaptations_.resolutions_adaptations;
    RTC_DCHECK_GE(adaptations_.resolutions_adaptations, 0);
  }

  bool CanDecreaseFrameRateTo(int max_frame_rate) {
    const int fps_wanted = std::max(kMinFramerateFps, max_frame_rate);
    return fps_wanted < rtc::dchecked_cast<int>(
                            source_restrictions_.max_frame_rate().value_or(
                                std::numeric_limits<int>::max()));
  }
  void DecreaseFrameRateTo(int max_frame_rate) {
    RTC_DCHECK(CanDecreaseFrameRateTo(max_frame_rate));
    max_frame_rate = std::max(kMinFramerateFps, max_frame_rate);
    RTC_LOG(LS_INFO) << "Scaling down framerate: " << max_frame_rate;
    source_restrictions_.set_max_frame_rate(
        max_frame_rate != std::numeric_limits<int>::max()
            ? absl::optional<double>(max_frame_rate)
            : absl::nullopt);
    ++adaptations_.fps_adaptations;
  }

  bool CanIncreaseFrameRateTo(int max_frame_rate) {
    return max_frame_rate > rtc::dchecked_cast<int>(
                                source_restrictions_.max_frame_rate().value_or(
                                    std::numeric_limits<int>::max()));
  }
  void IncreaseFrameRateTo(int max_frame_rate) {
    RTC_DCHECK(CanIncreaseFrameRateTo(max_frame_rate));
    RTC_LOG(LS_INFO) << "Scaling up framerate: " << max_frame_rate;
    source_restrictions_.set_max_frame_rate(
        max_frame_rate != std::numeric_limits<int>::max()
            ? absl::optional<double>(max_frame_rate)
            : absl::nullopt);
    --adaptations_.fps_adaptations;
    RTC_DCHECK_GE(adaptations_.fps_adaptations, 0);
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

  VideoSourceRestrictions source_restrictions_;
  AdaptationCounters adaptations_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoSourceRestrictor);
};

// static
VideoStreamAdaptor::AdaptationRequest::Mode
VideoStreamAdaptor::AdaptationRequest::GetModeFromAdaptationAction(
    VideoStreamAdaptor::AdaptationAction action) {
  switch (action) {
    case AdaptationAction::kIncreaseResolution:
      return AdaptationRequest::Mode::kAdaptUp;
    case AdaptationAction::kDecreaseResolution:
      return AdaptationRequest::Mode::kAdaptDown;
    case AdaptationAction::kIncreaseFrameRate:
      return AdaptationRequest::Mode::kAdaptUp;
    case AdaptationAction::kDecreaseFrameRate:
      return AdaptationRequest::Mode::kAdaptDown;
  }
}

VideoStreamAdaptor::VideoStreamAdaptor()
    : source_restrictor_(std::make_unique<VideoSourceRestrictor>()),
      balanced_settings_(),
      input_mode_(VideoInputMode::kNormalVideo),
      degradation_preference_(DegradationPreference::DISABLED),
      last_adaptation_request_(absl::nullopt) {}

VideoStreamAdaptor::~VideoStreamAdaptor() {}

VideoSourceRestrictions VideoStreamAdaptor::source_restrictions() {
  return source_restrictor_->source_restrictions();
}

const AdaptationCounters& VideoStreamAdaptor::adaptation_counters() const {
  return source_restrictor_->adaptation_counters();
}

void VideoStreamAdaptor::ClearRestrictions() {
  source_restrictor_->ClearRestrictions();
  last_adaptation_request_.reset();
}

const BalancedDegradationSettings& VideoStreamAdaptor::balanced_settings()
    const {
  return balanced_settings_;
}

void VideoStreamAdaptor::SetVideoInputMode(VideoInputMode input_mode) {
  input_mode_ = input_mode;
}

void VideoStreamAdaptor::SetDegradationPreference(
    DegradationPreference degradation_preference) {
  degradation_preference_ = degradation_preference;
}

DegradationPreference VideoStreamAdaptor::EffectiveDegradationPreference()
    const {
  // Balanced mode for screenshare works via automatic animation detection:
  // Resolution is capped for fullscreen animated content.
  // Adapatation is done only via framerate downgrade.
  // Thus effective degradation preference is MAINTAIN_RESOLUTION.
  // TODO(hbos): Don't do this. This is not what "balanced" means. If the
  // application wants to maintain resolution it should set that degradation
  // preference.
  return (input_mode_ == VideoInputMode::kScreenshareVideo &&
          degradation_preference_ == DegradationPreference::BALANCED)
             ? DegradationPreference::MAINTAIN_RESOLUTION
             : degradation_preference_;
}

// TODO(hbos): Make this an anonymous namespace function.
bool VideoStreamAdaptor::CanAdaptUpResolution(
    const absl::optional<EncoderSettings>& encoder_settings,
    absl::optional<uint32_t> encoder_target_bitrate_bps,
    int input_pixels) const {
  uint32_t bitrate_bps = encoder_target_bitrate_bps.value_or(0);
  absl::optional<VideoEncoder::ResolutionBitrateLimits> bitrate_limits =
      encoder_settings.has_value()
          ? encoder_settings->encoder_info()
                .GetEncoderBitrateLimitsForResolution(
                    VideoSourceRestrictor::GetHigherResolutionThan(
                        input_pixels))
          : absl::nullopt;
  if (!bitrate_limits.has_value() || bitrate_bps == 0) {
    return true;  // No limit configured or bitrate provided.
  }
  RTC_DCHECK_GE(bitrate_limits->frame_size_pixels, input_pixels);
  return bitrate_bps >=
         static_cast<uint32_t>(bitrate_limits->min_start_bitrate_bps);
}

absl::optional<VideoStreamAdaptor::AdaptationTarget>
VideoStreamAdaptor::GetAdaptUpTarget(
    const absl::optional<EncoderSettings>& encoder_settings,
    absl::optional<uint32_t> encoder_target_bitrate_bps,
    int input_pixels,
    int input_fps,
    AdaptationObserverInterface::AdaptReason reason) const {
  // Preconditions for being able to adapt up:
  if (input_mode_ == VideoInputMode::kNoVideo)
    return absl::nullopt;
  // 2. We shouldn't adapt up if we're currently waiting for a previous upgrade
  // to have an effect.
  // TODO(hbos): What about in the case of other degradation preferences?
  bool last_adaptation_was_up =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptUp;
  if (last_adaptation_was_up &&
      degradation_preference_ == DegradationPreference::MAINTAIN_FRAMERATE &&
      input_pixels <= last_adaptation_request_->input_pixel_count_) {
    return absl::nullopt;
  }
  // 3. We shouldn't adapt up if BalancedSettings doesn't allow it, which is
  // only applicable if reason is kQuality and preference is BALANCED.
  if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
      EffectiveDegradationPreference() == DegradationPreference::BALANCED &&
      !balanced_settings_.CanAdaptUp(
          GetVideoCodecTypeOrGeneric(encoder_settings), input_pixels,
          encoder_target_bitrate_bps.value_or(0))) {
    return absl::nullopt;
  }

  // Attempt to find an allowed adaptation target.
  switch (EffectiveDegradationPreference()) {
    case DegradationPreference::BALANCED: {
      // Attempt to increase target frame rate.
      int target_fps = balanced_settings_.MaxFps(
          GetVideoCodecTypeOrGeneric(encoder_settings), input_pixels);
      if (source_restrictor_->CanIncreaseFrameRateTo(target_fps)) {
        return AdaptationTarget(AdaptationAction::kIncreaseFrameRate,
                                target_fps);
      }
      // Fall-through to maybe-adapting resolution, unless |balanced_settings_|
      // forbids it based on bitrate.
      if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
          !balanced_settings_.CanAdaptUpResolution(
              GetVideoCodecTypeOrGeneric(encoder_settings), input_pixels,
              encoder_target_bitrate_bps.value_or(0))) {
        return absl::nullopt;
      }
      // Scale up resolution.
      ABSL_FALLTHROUGH_INTENDED;
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Don't adapt resolution if CanAdaptUpResolution() forbids it based on
      // bitrate and limits specified by encoder capabilities.
      if (reason == AdaptationObserverInterface::AdaptReason::kQuality &&
          !CanAdaptUpResolution(encoder_settings, encoder_target_bitrate_bps,
                                input_pixels)) {
        return absl::nullopt;
      }
      // Attempt to increase pixel count.
      int target_pixels = input_pixels;
      if (source_restrictor_->adaptation_counters().resolutions_adaptations ==
          1) {
        RTC_LOG(LS_INFO) << "Removing resolution down-scaling setting.";
        target_pixels = std::numeric_limits<int>::max();
      }
      target_pixels =
          VideoSourceRestrictor::GetHigherResolutionThan(target_pixels);
      if (!source_restrictor_->CanIncreaseResolutionTo(target_pixels))
        return absl::nullopt;
      return AdaptationTarget(AdaptationAction::kIncreaseResolution,
                              target_pixels);
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      // Scale up framerate.
      int target_fps = input_fps;
      if (source_restrictor_->adaptation_counters().fps_adaptations == 1) {
        RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
        target_fps = std::numeric_limits<int>::max();
      }
      target_fps = VideoSourceRestrictor::GetHigherFrameRateThan(target_fps);
      if (!source_restrictor_->CanIncreaseFrameRateTo(target_fps))
        return absl::nullopt;
      return AdaptationTarget(AdaptationAction::kIncreaseFrameRate, target_fps);
    }
    case DegradationPreference::DISABLED:
      return absl::nullopt;
  }
}

absl::optional<VideoStreamAdaptor::AdaptationTarget>
VideoStreamAdaptor::GetAdaptDownTarget(
    const absl::optional<EncoderSettings>& encoder_settings,
    int input_pixels,
    int input_fps,
    int min_pixels_per_frame,
    VideoStreamEncoderObserver* encoder_stats_observer) const {
  // Preconditions for being able to adapt down:
  if (input_mode_ == VideoInputMode::kNoVideo)
    return absl::nullopt;
  // 1. We are not disabled.
  // TODO(hbos): Don't support DISABLED, it doesn't exist in the spec and it
  // causes scaling due to bandwidth constraints (QualityScalerResource) to be
  // ignored, not just CPU signals. This is not a use case we want to support;
  // remove the enum value.
  if (degradation_preference_ == DegradationPreference::DISABLED)
    return absl::nullopt;
  bool last_adaptation_was_down =
      last_adaptation_request_ &&
      last_adaptation_request_->mode_ == AdaptationRequest::Mode::kAdaptDown;
  // 2. We shouldn't adapt down if our frame rate is below the minimum or if its
  // currently unknown.
  if (EffectiveDegradationPreference() ==
      DegradationPreference::MAINTAIN_RESOLUTION) {
    // TODO(hbos): This usage of |last_adaptation_was_down| looks like a mistake
    // - delete it.
    if (input_fps <= 0 || (last_adaptation_was_down &&
                           input_fps < VideoStreamAdaptor::kMinFramerateFps)) {
      return absl::nullopt;
    }
  }
  // 3. We shouldn't adapt down if we're currently waiting for a previous
  // downgrade to have an effect.
  // TODO(hbos): What about in the case of other degradation preferences?
  if (last_adaptation_was_down &&
      degradation_preference_ == DegradationPreference::MAINTAIN_FRAMERATE &&
      input_pixels >= last_adaptation_request_->input_pixel_count_) {
    return absl::nullopt;
  }

  // Attempt to find an allowed adaptation target.
  switch (EffectiveDegradationPreference()) {
    case DegradationPreference::BALANCED: {
      // Try scale down framerate, if lower.
      int target_fps = balanced_settings_.MinFps(
          GetVideoCodecTypeOrGeneric(encoder_settings), input_pixels);
      if (source_restrictor_->CanDecreaseFrameRateTo(target_fps)) {
        return AdaptationTarget(AdaptationAction::kDecreaseFrameRate,
                                target_fps);
      }
      // Scale down resolution.
      ABSL_FALLTHROUGH_INTENDED;
    }
    case DegradationPreference::MAINTAIN_FRAMERATE: {
      // Scale down resolution.
      int target_pixels =
          VideoSourceRestrictor::GetLowerResolutionThan(input_pixels);
      // TODO(https://crbug.com/webrtc/11222): Move this logic to
      // ApplyAdaptationTarget() or elsewhere - simply checking which adaptation
      // target is available should not have side-effects.
      if (target_pixels < min_pixels_per_frame)
        encoder_stats_observer->OnMinPixelLimitReached();
      if (!source_restrictor_->CanDecreaseResolutionTo(target_pixels,
                                                       min_pixels_per_frame)) {
        return absl::nullopt;
      }
      return AdaptationTarget(AdaptationAction::kDecreaseResolution,
                              target_pixels);
    }
    case DegradationPreference::MAINTAIN_RESOLUTION: {
      int target_fps = VideoSourceRestrictor::GetLowerFrameRateThan(input_fps);
      if (!source_restrictor_->CanDecreaseFrameRateTo(target_fps))
        return absl::nullopt;
      return AdaptationTarget(AdaptationAction::kDecreaseFrameRate, target_fps);
    }
    case DegradationPreference::DISABLED:
      RTC_NOTREACHED();
      return absl::nullopt;
  }
}

void VideoStreamAdaptor::ApplyAdaptationTarget(const AdaptationTarget& target,
                                               int input_pixels,
                                               int input_fps,
                                               int min_pixels_per_frame) {
  last_adaptation_request_.emplace(AdaptationRequest{
      input_pixels, input_fps,
      AdaptationRequest::GetModeFromAdaptationAction(target.action)});
  switch (target.action) {
    case AdaptationAction::kIncreaseResolution:
      source_restrictor_->IncreaseResolutionTo(target.value);
      return;
    case AdaptationAction::kDecreaseResolution:
      source_restrictor_->DecreaseResolutionTo(target.value,
                                               min_pixels_per_frame);
      return;
    case AdaptationAction::kIncreaseFrameRate:
      source_restrictor_->IncreaseFrameRateTo(target.value);
      // TODO(https://crbug.com/webrtc/11222): Don't adapt in two steps.
      // GetAdaptUpTarget() should tell us the correct value, but BALANCED logic
      // in DecrementFramerate() makes it hard to predict whether this will be
      // the last step. Remove the dependency on GetConstAdaptCounter().
      if (EffectiveDegradationPreference() == DegradationPreference::BALANCED &&
          source_restrictor_->adaptation_counters().fps_adaptations == 0 &&
          target.value != std::numeric_limits<int>::max()) {
        RTC_LOG(LS_INFO) << "Removing framerate down-scaling setting.";
        source_restrictor_->IncreaseFrameRateTo(
            std::numeric_limits<int>::max());
      }
      return;
    case AdaptationAction::kDecreaseFrameRate:
      source_restrictor_->DecreaseFrameRateTo(target.value);
      return;
  }
}

}  // namespace webrtc
