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
// BeginSetSource/CompleteSetSource) or when comitting new sink wants (with
// CommitSinkWants).
//
// This class is thread safe due to semantics by which SetSource is called. All
// other API has to be called on the same sequence.
class VideoSourceSinkController {
 public:
  // The return value of SetSource, used for CompleteSetSource.
  class Completion {
   private:
    Completion(rtc::VideoSourceInterface<VideoFrame>* source,
               int sequence_number)
        : source(source), sequence_number(sequence_number) {}

    friend class VideoSourceSinkController;
    rtc::VideoSourceInterface<VideoFrame>* const source;  // The new source.
    const int sequence_number;  // The sequence number in effect when
                                // BeginSetSource was executed.
  };

  VideoSourceSinkController(const SinkWantsCalculator& calculator,
                            rtc::VideoSinkInterface<VideoFrame>* sink);

  // Clears the source. If a source was set, |sink| is de-registered from it
  // prior to clearing.
  void ClearSource();

  // Begins setting a new source. If a source was set, |sink| is de-registered
  // from it prior to clearing.
  // Returns an opaque value which must be subsequently used to call
  // CompleteSetSource.
  // NOTE: Only use if |source| isn't nullptr - in that case just use
  // ClearSource() instead.
  ABSL_MUST_USE_RESULT Completion
  BeginSetSource(rtc::VideoSourceInterface<VideoFrame>* source);

  // Completes a SetSource operation. The AddOrUpdateSink method of the source
  // set from BeginSetSource is called with the calculator's current sink wants.
  // Should be called after SetSource has been called to complete the SetSource
  // operation.
  // The method is expected to be called on the same sequence that the
  // SinkWantsCalculator is used on.
  void CompleteSetSource(Completion set_source_result);

  // Call to change sink wants of the current source.
  void CommitSinkWants();

 private:
  mutable Mutex mutex_;

  const SinkWantsCalculator& calculator_;
  rtc::VideoSinkInterface<VideoFrame>* const sink_;
  rtc::VideoSourceInterface<VideoFrame>* source_ RTC_GUARDED_BY(&mutex_) =
      nullptr;
  // Sequence number used to detect that BeginSetSource was called again before
  // CompleteSetSource was called, in which case the completion is ignored
  // (because the original source might be gone).
  int begin_set_source_sequence_number_ RTC_GUARDED_BY(&mutex_) = 0;
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_SOURCE_SINK_CONTROLLER_H_
