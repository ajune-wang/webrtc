/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_FRAMES_COMPARATOR_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_FRAMES_COMPARATOR_H_

#include <deque>
#include <map>
#include <utility>
#include <vector>

#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "system_wrappers/include/clock.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_cpu_measurer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_internal_shared_objects.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_shared_objects.h"

namespace webrtc {

struct DefaultVideoQualityAnalyzerFramesComparatorOptions {
  // Tells DefaultVideoQualityAnalyzer if heavy metrics like PSNR and SSIM have
  // to be computed or not.
  bool heavy_metrics_computation_enabled = true;
  // If true DefaultVideoQualityAnalyzer will try to adjust frames before
  // computing PSNR and SSIM for them. In some cases picture may be shifted by
  // a few pixels after the encode/decode step. Those difference is invisible
  // for a human eye, but it affects the metrics. So the adjustment is used to
  // get metrics that are closer to how human percepts the video. This feature
  // significantly slows down the comparison, so turn it on only when it is
  // needed.
  bool adjust_cropping_before_comparing_frames = false;
  // If true, the analyzer will expect peers to receive their own video streams.
  bool enable_receive_own_stream = false;
};

struct FramesComparatorStats {
  // Size of analyzer internal comparisons queue, measured when new element
  // id added to the queue.
  SamplesStatsCounter comparisons_queue_size;
  // Number of performed comparisons of 2 video frames from captured and
  // rendered streams.
  int64_t comparisons_done = 0;
  // Number of cpu overloaded comparisons. Comparison is cpu overloaded if it is
  // queued when there are too many not processed comparisons in the queue.
  // Overloaded comparison doesn't include metrics like SSIM and PSNR that
  // require heavy computations.
  int64_t cpu_overloaded_comparisons_done = 0;
  // Number of memory overloaded comparisons. Comparison is memory overloaded if
  // it is queued when its captured frame was already removed due to high memory
  // usage for that video stream.
  int64_t memory_overloaded_comparisons_done = 0;
};

class DefaultVideoQualityAnalyzerFramesComparator {
 public:
  explicit DefaultVideoQualityAnalyzerFramesComparator(
      webrtc::Clock* clock,
      DefaultVideoQualityAnalyzerCpuMeasurer& cpu_measurer,
      DefaultVideoQualityAnalyzerFramesComparatorOptions options = {})
      : options_(options), clock_(clock), cpu_measurer_(cpu_measurer) {}
  ~DefaultVideoQualityAnalyzerFramesComparator() { Stop(); }

  void Start(int max_threads_count);
  void Stop();

  // Ensures that stream `stream_index` has stats objects created for all
  // potential receivers.
  void EnsureStatsForStream(size_t stream_index,
                            size_t peer_index,
                            size_t peers_count,
                            Timestamp captured_time,
                            Timestamp start_time);
  void RegisterParticipantInCall(
      rtc::ArrayView<std::pair<InternalStatsKey, Timestamp>>
          stream_stats_to_add,
      Timestamp start_time);

  void AddTimeBetweenFreezes(InternalStatsKey key,
                             SamplesStatsCounter::StatsSample sample);

  // `skipped_between_rendered` - amount of frames dropped on this stream before
  // last received frame and current frame.
  void AddComparison(InternalStatsKey stats_key,
                     int skipped_between_rendered,
                     absl::optional<VideoFrame> captured,
                     absl::optional<VideoFrame> rendered,
                     bool dropped,
                     FrameStats frame_stats);
  void AddComparison(InternalStatsKey stats_key,
                     absl::optional<VideoFrame> captured,
                     absl::optional<VideoFrame> rendered,
                     bool dropped,
                     FrameStats frame_stats);

  std::map<InternalStatsKey, webrtc_pc_e2e::StreamStats> stream_stats() const {
    MutexLock lock(&mutex_);
    return stream_stats_;
  }
  std::map<InternalStatsKey, Timestamp> stream_last_freeze_end_time() const {
    MutexLock lock(&mutex_);
    return stream_last_freeze_end_time_;
  }
  FramesComparatorStats frames_comparator_stats() const {
    MutexLock lock(&mutex_);
    return frames_comparator_stats_;
  }

 private:
  enum State { kNew, kActive, kStopped };

  void AddComparisonInternal(InternalStatsKey stats_key,
                             absl::optional<VideoFrame> captured,
                             absl::optional<VideoFrame> rendered,
                             bool dropped,
                             FrameStats frame_stats)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void ProcessComparisons();
  void ProcessComparison(const FrameComparison& comparison);
  Timestamp Now();

  const DefaultVideoQualityAnalyzerFramesComparatorOptions options_;
  webrtc::Clock* const clock_;
  DefaultVideoQualityAnalyzerCpuMeasurer& cpu_measurer_;

  mutable Mutex mutex_;
  State state_ RTC_GUARDED_BY(mutex_) = State::kNew;
  std::map<InternalStatsKey, webrtc_pc_e2e::StreamStats> stream_stats_
      RTC_GUARDED_BY(mutex_);
  std::map<InternalStatsKey, Timestamp> stream_last_freeze_end_time_
      RTC_GUARDED_BY(mutex_);
  std::deque<FrameComparison> comparisons_ RTC_GUARDED_BY(mutex_);
  FramesComparatorStats frames_comparator_stats_ RTC_GUARDED_BY(mutex_);

  std::vector<rtc::PlatformThread> thread_pool_;
  rtc::Event comparison_available_event_;
};

}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_FRAMES_COMPARATOR_H_
