/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_SOURCE_SINK_CONTROLLER_H_
#define VIDEO_VIDEO_SOURCE_SINK_CONTROLLER_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/sequence_checker.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "call/adaptation/video_source_restrictions.h"
#include "rtc_base/system/no_unique_address.h"

namespace webrtc {

// Class for accumulating and calculating sink wants.
// With the exception of the ctor, this class need to be used entirely from the
// same sequence.
class SinkWantsCalculator {
 public:
  SinkWantsCalculator();

  rtc::VideoSinkWants ComputeWants() const;

  VideoSourceRestrictions restrictions() const;
  absl::optional<size_t> pixels_per_frame_upper_limit() const;
  absl::optional<double> frame_rate_upper_limit() const;
  bool rotation_applied() const;
  int resolution_alignment() const;
  const std::vector<rtc::VideoSinkWants::FrameSize>& resolutions() const;

  // Updates the settings stored internally. In order for these settings to be
  // applied to the sink, PushSourceSinkSettings() must subsequently be called.
  void SetRestrictions(VideoSourceRestrictions restrictions);
  void SetPixelsPerFrameUpperLimit(
      absl::optional<size_t> pixels_per_frame_upper_limit);
  void SetFrameRateUpperLimit(absl::optional<double> frame_rate_upper_limit);
  void SetRotationApplied(bool rotation_applied);
  void SetResolutionAlignment(int resolution_alignment);
  void SetResolutions(std::vector<rtc::VideoSinkWants::FrameSize> resolutions);

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
  // Pixel and frame rate restrictions.
  VideoSourceRestrictions restrictions_ RTC_GUARDED_BY(&sequence_checker_);
  // Ensures that even if we are not restricted, the sink is never configured
  // above this limit. Example: We are not CPU limited (no `restrictions_`) but
  // our encoder is capped at 30 fps (= `frame_rate_upper_limit_`).
  absl::optional<size_t> pixels_per_frame_upper_limit_
      RTC_GUARDED_BY(&sequence_checker_);
  absl::optional<double> frame_rate_upper_limit_
      RTC_GUARDED_BY(&sequence_checker_);
  bool rotation_applied_ RTC_GUARDED_BY(&sequence_checker_) = false;
  int resolution_alignment_ RTC_GUARDED_BY(&sequence_checker_) = 1;
  std::vector<rtc::VideoSinkWants::FrameSize> resolutions_
      RTC_GUARDED_BY(&sequence_checker_);
};

// Responsible for configuring source/sink settings, i.e. performing
// rtc::VideoSourceInterface<VideoFrame>::AddOrUpdateSink(). It does this by
// quering the referenced SinkWantsCalculator when switching sources (with
// SetSource/UpdateSinkAndWants) or when comitting new sink wants (with
// only UpdateSinkAndWants).
//
// This class is thread safe, except UpdateSinkAndWants needs to be called on
// the same sequence as operates on the SinkWantsCalculator.
class VideoSourceSinkController {
 public:
  VideoSourceSinkController(const SinkWantsCalculator& calculator,
                            rtc::VideoSinkInterface<VideoFrame>* sink);

  // Begins setting a new source. If the old source was already set and
  // different from |source|, |sink| is de-registered from it prior to clearing.
  void SetSource(rtc::VideoSourceInterface<VideoFrame>* source);

  // Completes a SetSource operation and updates sink wants of the current
  // source. It's fine to call this method without prior SetSource operation, to
  // commit changes in the registered SinkWantsCalculator. The AddOrUpdateSink
  // method of the source set from SetSource is called with the
  // calculator's current sink wants.
  // The method is expected to be called on the same sequence that the
  // SinkWantsCalculator is used on.
  void UpdateSinkAndWants();

 private:
  Mutex mutex_;
  const SinkWantsCalculator& calculator_;
  rtc::VideoSinkInterface<VideoFrame>* const sink_;
  rtc::VideoSourceInterface<VideoFrame>* source_ RTC_GUARDED_BY(&mutex_) =
      nullptr;
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_SOURCE_SINK_CONTROLLER_H_
