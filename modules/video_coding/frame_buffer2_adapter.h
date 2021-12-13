/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_FRAME_BUFFER2_ADAPTER_H_
#define MODULES_VIDEO_CODING_FRAME_BUFFER2_ADAPTER_H_

#include <array>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/video/encoded_frame.h"
#include "modules/video_coding/frame_buffer2_interface.h"
#include "modules/video_coding/frame_scheduler.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/inter_frame_delay.h"
#include "modules/video_coding/jitter_estimator.h"
#include "modules/video_coding/utility/decoded_frames_history.h"
#include "rtc_base/event.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/rtt_mult_experiment.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class Clock;
class VCMReceiveStatisticsCallback;
class VCMJitterEstimator;
class VCMTiming;

// Adapts the FrameBuffer API but backed with FrameBuffer3.
class FrameBuffer2Adapter : public FrameBuffer2Interface {
 public:
  FrameBuffer2Adapter(DecodeStreamTimeouts timeouts,
                      rtc::TaskQueue* task_queue,
                      Clock* clock,
                      VCMTiming* timing,
                      VCMReceiveStatisticsCallback* stats_callback);

  FrameBuffer2Adapter() = delete;
  FrameBuffer2Adapter(const FrameBuffer2Adapter&) = delete;
  FrameBuffer2Adapter& operator=(const FrameBuffer2Adapter&) = delete;

  ~FrameBuffer2Adapter() override;

  // Insert a frame into the frame buffer. Returns the picture id
  // of the last continuous frame or -1 if there is no continuous frame.
  int64_t InsertFrame(std::unique_ptr<EncodedFrame> frame) override;

  // Get the next frame for decoding. Will return at latest after
  // `max_wait_time_ms`.
  void NextFrame(bool keyframe_required,
                 rtc::TaskQueue* callback_queue,
                 FrameBuffer2Interface::NextFrameCallback handler) override;

  // Tells the FrameBuffer which protection mode that is in use. Affects
  // the frame timing.
  // TODO(philipel): Remove this when new timing calculations has been
  //                 implemented.
  void SetProtectionMode(VCMVideoProtection mode) override;

  // Stop the frame buffer, causing any sleeping thread in NextFrame to
  // return immediately.
  void Stop() override;

  // Updates the RTT for jitter buffer estimation.
  void UpdateRtt(int64_t rtt_ms) override;

  // Clears the FrameBuffer, removing all the buffered frames.
  void Clear() override;

  int Size() override;

 private:
  std::unique_ptr<EncodedFrame> CombineFrames(
      absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames_to_decode,
      int64_t render_time) RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void OnTimeout();
  void OnFrameReady(
      absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames);
  void UpdateJitterDelay();
  void UpdateTimingFrameInfo();
  bool HasBadRenderTiming(int64_t render_time_ms, int64_t now_ms) const;
  // TODO(eshr): Move to common place.
  std::unique_ptr<EncodedFrame> CombineAndDeleteFrames(
      absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4> frames) const;

  void StopOnBookkeepingQueue() RTC_RUN_ON(bookkeeping_queue_);

  Mutex mutex_;

  Clock* const clock_;
  FrameBuffer frame_buffer_ RTC_GUARDED_BY(bookkeeping_queue_);
  int dropped_frames_before_ RTC_GUARDED_BY(bookkeeping_queue_) = 0;
  VCMTiming* const timing_;
  VCMJitterEstimator jitter_estimator_ RTC_GUARDED_BY(mutex_);
  VCMInterFrameDelay inter_frame_delay_ RTC_GUARDED_BY(mutex_);
  VCMVideoProtection protection_mode_ RTC_GUARDED_BY(mutex_);
  VCMReceiveStatisticsCallback* const stats_callback_;
  rtc::TaskQueue* const bookkeeping_queue_;
  rtc::TaskQueue* callback_queue_ RTC_GUARDED_BY(mutex_) = nullptr;
  FrameBuffer2Interface::NextFrameCallback frame_handler_
      RTC_GUARDED_BY(mutex_);
  bool stopped_ RTC_GUARDED_BY(mutex_) = false;
  bool has_started_ RTC_GUARDED_BY(mutex_) = false;
  FrameScheduler scheduler_ RTC_GUARDED_BY(bookkeeping_queue_);

  // rtt_mult experiment settings.
  const absl::optional<RttMultExperiment::Settings> rtt_mult_settings_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_FRAME_BUFFER2_ADAPTER_H_
