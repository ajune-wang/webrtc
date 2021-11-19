/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_source_sink_controller.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

namespace {

std::string WantsToString(const rtc::VideoSinkWants& wants) {
  rtc::StringBuilder ss;

  ss << "max_fps=" << wants.max_framerate_fps
     << " max_pixel_count=" << wants.max_pixel_count << " target_pixel_count="
     << (wants.target_pixel_count.has_value()
             ? std::to_string(wants.target_pixel_count.value())
             : "null")
     << " resolutions={";
  for (size_t i = 0; i < wants.resolutions.size(); ++i) {
    if (i != 0)
      ss << ",";
    ss << wants.resolutions[i].width << "x" << wants.resolutions[i].height;
  }
  ss << "}";

  return ss.Release();
}

}  // namespace

SinkWantsCalculator::SinkWantsCalculator() {
  sequence_checker_.Detach();
}

rtc::VideoSinkWants SinkWantsCalculator::ComputeWants() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  rtc::VideoSinkWants wants;
  wants.rotation_applied = rotation_applied_;
  // `wants.black_frames` is not used, it always has its default value false.
  wants.max_pixel_count =
      rtc::dchecked_cast<int>(restrictions_.max_pixels_per_frame().value_or(
          std::numeric_limits<int>::max()));
  wants.target_pixel_count =
      restrictions_.target_pixels_per_frame().has_value()
          ? absl::optional<int>(rtc::dchecked_cast<int>(
                restrictions_.target_pixels_per_frame().value()))
          : absl::nullopt;
  wants.max_framerate_fps =
      restrictions_.max_frame_rate().has_value()
          ? static_cast<int>(restrictions_.max_frame_rate().value())
          : std::numeric_limits<int>::max();
  wants.resolution_alignment = resolution_alignment_;
  wants.max_pixel_count =
      std::min(wants.max_pixel_count,
               rtc::dchecked_cast<int>(pixels_per_frame_upper_limit_.value_or(
                   std::numeric_limits<int>::max())));
  wants.max_framerate_fps =
      std::min(wants.max_framerate_fps,
               frame_rate_upper_limit_.has_value()
                   ? static_cast<int>(frame_rate_upper_limit_.value())
                   : std::numeric_limits<int>::max());
  wants.resolutions = resolutions_;
  return wants;
}

VideoSourceRestrictions SinkWantsCalculator::restrictions() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return restrictions_;
}

absl::optional<size_t> SinkWantsCalculator::pixels_per_frame_upper_limit()
    const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return pixels_per_frame_upper_limit_;
}

absl::optional<double> SinkWantsCalculator::frame_rate_upper_limit() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return frame_rate_upper_limit_;
}

bool SinkWantsCalculator::rotation_applied() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return rotation_applied_;
}

int SinkWantsCalculator::resolution_alignment() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return resolution_alignment_;
}

const std::vector<rtc::VideoSinkWants::FrameSize>&
SinkWantsCalculator::resolutions() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return resolutions_;
}

void SinkWantsCalculator::SetRestrictions(
    VideoSourceRestrictions restrictions) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  restrictions_ = std::move(restrictions);
}

void SinkWantsCalculator::SetPixelsPerFrameUpperLimit(
    absl::optional<size_t> pixels_per_frame_upper_limit) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  pixels_per_frame_upper_limit_ = std::move(pixels_per_frame_upper_limit);
}

void SinkWantsCalculator::SetFrameRateUpperLimit(
    absl::optional<double> frame_rate_upper_limit) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  frame_rate_upper_limit_ = std::move(frame_rate_upper_limit);
}

void SinkWantsCalculator::SetRotationApplied(bool rotation_applied) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  rotation_applied_ = rotation_applied;
}

void SinkWantsCalculator::SetResolutionAlignment(int resolution_alignment) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  resolution_alignment_ = resolution_alignment;
}

void SinkWantsCalculator::SetResolutions(
    std::vector<rtc::VideoSinkWants::FrameSize> resolutions) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  resolutions_ = std::move(resolutions);
}

VideoSourceSinkController::VideoSourceSinkController(
    const SinkWantsCalculator& calculator,
    rtc::VideoSinkInterface<VideoFrame>* sink)
    : calculator_(calculator), sink_(sink) {
  RTC_DCHECK(sink_);
}

void VideoSourceSinkController::ClearSource() {
  MutexLock lock(&mutex_);
  if (source_) {
    source_->RemoveSink(sink_);
    source_ = nullptr;
  }
}

VideoSourceSinkController::Completion VideoSourceSinkController::BeginSetSource(
    rtc::VideoSourceInterface<VideoFrame>* source) {
  RTC_DCHECK(source);
  MutexLock lock(&mutex_);
  if (source_ && source_ != source) {
    source_->RemoveSink(sink_);
    source_ = nullptr;
  }
  ++begin_set_source_sequence_number_;
  return Completion(source, begin_set_source_sequence_number_);
}

void VideoSourceSinkController::CompleteSetSource(
    Completion set_source_result) {
  rtc::VideoSinkWants wants = calculator_.ComputeWants();
  MutexLock lock(&mutex_);
  // Bail out if we know another call switched the source.
  if (set_source_result.sequence_number != begin_set_source_sequence_number_)
    return;
  source_ = set_source_result.source;
  source_->AddOrUpdateSink(sink_, wants);
}

void VideoSourceSinkController::CommitSinkWants() {
  rtc::VideoSinkWants wants = calculator_.ComputeWants();
  MutexLock lock(&mutex_);
  if (!source_)
    return;
  RTC_LOG(LS_INFO) << "Pushing SourceSink restrictions: "
                   << WantsToString(wants);
  source_->AddOrUpdateSink(sink_, wants);
}

}  // namespace webrtc
