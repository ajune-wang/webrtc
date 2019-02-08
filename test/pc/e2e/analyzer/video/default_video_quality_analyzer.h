/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_H_

#include <atomic>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/event.h"
#include "rtc_base/numerics/samples_stats_counter.h"
#include "rtc_base/platform_thread.h"
#include "system_wrappers/include/clock.h"
#include "test/pc/e2e/api/video_quality_analyzer_interface.h"

namespace webrtc {
namespace test {

class RateCounter {
 public:
  void AddEvent(Timestamp event_time) {
    if (event_first_time_.IsMinusInfinity()) {
      event_first_time_ = event_time;
    }
    event_last_time_ = event_time;
    event_count_++;
  }

  bool IsEmpty() const { return event_first_time_ == event_last_time_; }

  double GetEventsPerSecond() const {
    RTC_DCHECK(!IsEmpty());
    return static_cast<double>(event_count_) /
           (event_last_time_ - event_first_time_).seconds();
  }

 private:
  Timestamp event_first_time_ = Timestamp::MinusInfinity();
  Timestamp event_last_time_ = Timestamp::MinusInfinity();
  uint64_t event_count_ = 0;
};

struct FrameCounters {
  uint64_t captured = 0;
  uint64_t pre_encoded = 0;
  uint64_t encoded = 0;
  uint64_t received = 0;
  uint64_t decoded = 0;
  uint64_t rendered = 0;
  uint64_t dropped = 0;
};

struct StreamStats {
 public:
  SamplesStatsCounter psnr;
  SamplesStatsCounter ssim;
  // time from packet encoded to the packet received in decoder.
  SamplesStatsCounter transport_time_ms;
  // time from frame was captured on device to time frame was displayed on
  // device
  SamplesStatsCounter total_delay_incl_transport_ms;
  // time between frames out from renderer
  SamplesStatsCounter time_between_rendered_frames_ms;
  RateCounter encode_frame_rate;
  SamplesStatsCounter encode_time_ms;
  SamplesStatsCounter decode_time_ms;
  // max frames skipped between two nearest
  SamplesStatsCounter skipped_between_rendered;
  // mean time from previous freeze end to new freeze start (freeze - no new
  // frames from decoder for 150ms + avg time between frames or 3 * avg time
  // between frames)
  SamplesStatsCounter time_between_freezes_ms;
  SamplesStatsCounter freeze_time_ms;
  // A sum of freezes duration, where freeze is a pause over 200ms without new
  // frames rendered.
  uint64_t freeze_200_ms = 0;
  // A sum of freezes duration, where freeze is a pause over 1s without new
  // frames rendered.
  uint64_t freeze_1s = 0;
  // a mean of the resolutions
  SamplesStatsCounter encoded_pix;
  uint64_t dropped_by_encoder = 0;
  uint64_t dropped_before_encoder = 0;
  uint64_t comparisons_done = 0;
  uint64_t overloaded_comparisons_done = 0;
};

struct AnalyzerStats {
 public:
  // Size of analyzer internal comparisons queue, measured when new element
  // added to the queue.
  SamplesStatsCounter comparisons_queue_size;
};

class DefaultVideoQualityAnalyzer : public VideoQualityAnalyzerInterface {
 public:
  explicit DefaultVideoQualityAnalyzer(std::string test_label);
  ~DefaultVideoQualityAnalyzer() override;

  void Start(int max_threads_count) override;
  uint16_t OnFrameCaptured(const std::string& stream_label,
                           const VideoFrame& frame) override;
  void OnFramePreEncode(const VideoFrame& frame) override;
  void OnFrameEncoded(uint16_t frame_id,
                      const EncodedImage& encoded_image) override;
  void OnFrameDropped(EncodedImageCallback::DropReason reason) override;
  void OnFrameReceived(uint16_t frame_id,
                       const EncodedImage& input_image) override;
  void OnFrameDecoded(const VideoFrame& frame,
                      absl::optional<int32_t> decode_time_ms,
                      absl::optional<uint8_t> qp) override;
  void OnFrameRendered(const VideoFrame& frame) override;
  void OnEncoderError(const VideoFrame& frame, int32_t error_code) override;
  void OnDecoderError(uint16_t frame_id, int32_t error_code) override;
  void Stop() override;

  // TODO(titovartem) Maybe analyzer should always report stats for all known
  // streams?
  // These two methods will be part of public API for this analyzer
  // Returns set of stream labels, that were met during test call.
  std::set<std::string> GetKnownVideoStreams() const;
  // Report results with a standard way for this stream label.
  void ReportResults(const std::string& stream_label) const;

  FrameCounters GetGlobalCounters();
  std::map<std::string, FrameCounters> GetPerStreamCounters() const;
  // Returns video quality stats per stream.
  std::map<std::string, StreamStats> GetStats() const;
  AnalyzerStats GetAnalyzerStats() const;

 private:
  struct FrameStats {
    FrameStats(std::string stream_label, Timestamp captured_time);

    std::string stream_label;

    // Frame events timestamp
    Timestamp captured_time;
    Timestamp pre_encode_time = Timestamp::MinusInfinity();
    Timestamp encoded_time = Timestamp::MinusInfinity();
    Timestamp received_time = Timestamp::MinusInfinity();
    Timestamp decoded_time = Timestamp::MinusInfinity();
    Timestamp rendered_time = Timestamp::MinusInfinity();
    Timestamp prev_frame_rendered_time = Timestamp::MinusInfinity();

    size_t encoded_frame_size = 0;
    absl::optional<int32_t> decoder_reported_time_ms = absl::nullopt;
    absl::optional<uint8_t> decoder_reported_qp = absl::nullopt;
    absl::optional<int> rendered_frame_width = absl::nullopt;
    absl::optional<int> rendered_frame_height = absl::nullopt;
  };

  struct FrameComparison {
    FrameComparison(absl::optional<VideoFrame> captured,
                    absl::optional<VideoFrame> rendered,
                    bool dropped,
                    FrameStats frame_stats);
    FrameComparison(bool dropped, FrameStats frameStats);

    // Frames can be omitted if there too many computations waiting in the
    // queue.
    absl::optional<VideoFrame> captured;
    absl::optional<VideoFrame> rendered;
    bool dropped;
    FrameStats frame_stats;
  };

  enum State { kNew, kActive, kStopped };

  // Returns last rendered frame for stream if there is one or nullptr
  // otherwise.
  VideoFrame* GetLastRenderedFrame(const std::string& stream_label)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void SetLastRenderedFrame(const std::string& stream_label,
                            const VideoFrame& frame)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void AddComparison(absl::optional<VideoFrame> captured,
                     absl::optional<VideoFrame> rendered,
                     bool dropped,
                     FrameStats frame_stats);
  static void ProcessComparisonsThread(void* obj);
  void ProcessComparisons();
  void ProcessComparison(const FrameComparison& comparison);
  void ReportResult(const std::string& metric_name,
                    const std::string& stream_label,
                    const SamplesStatsCounter& counter,
                    const std::string& unit) const;
  std::string GetTraceName(const std::string& stream_label) const;
  Timestamp Now();

  const std::string test_label_;

  webrtc::Clock* const clock_;
  std::atomic<uint16_t> next_frame_id_{0};

  rtc::CriticalSection lock_;
  State state_ RTC_GUARDED_BY(lock_) = State::kNew;
  // Frames that were captured by all streams and still aren't rendered by any
  // stream.
  std::map<uint16_t, VideoFrame> captured_frames_in_flight_
      RTC_GUARDED_BY(lock_);
  // Global frames count for all video streams.
  FrameCounters frame_counters_ RTC_GUARDED_BY(lock_);
  // Frame counters per each stream.
  std::map<std::string, FrameCounters> stream_frame_counters_
      RTC_GUARDED_BY(lock_);
  std::map<uint16_t, FrameStats> frame_stats_ RTC_GUARDED_BY(lock_);

  // To correctly determine dropped frames we have to know sequence of frames
  // in each stream so we will keep a mapping from stream label to list of
  // frame ids inside the stream. When the frame is rendered, we will pop ids
  // for the list for this stream until id will match with rendered one. All ids
  // before matched one can be considered as dropped:
  //
  // stream1 <--> | frame_id1 |->| frame_id2 |->| frame_id3 |->| frame_id4 |
  //
  // If we received frame with id frame_id3, then we will pop frame_id1 and
  // frame_id2 and consider that frames as dropped and then compare received
  // frame with the one from |captured_frames_in_flight_| with id frame_id3.
  // Also we will put it to the |stream_last_rendered_frame_|
  std::map<std::string, std::list<uint16_t>> stream_frame_id_list_
      RTC_GUARDED_BY(lock_);
  std::map<std::string, VideoFrame> stream_last_rendered_frame_
      RTC_GUARDED_BY(lock_);
  std::map<std::string, Timestamp> stream_last_rendered_frame_time_
      RTC_GUARDED_BY(lock_);

  rtc::CriticalSection comparison_lock_;
  std::map<std::string, StreamStats> stream_stats_
      RTC_GUARDED_BY(comparison_lock_);
  std::map<std::string, Timestamp> stream_last_freeze_end_time_
      RTC_GUARDED_BY(comparison_lock_);
  std::deque<FrameComparison> comparisons_ RTC_GUARDED_BY(comparison_lock_);
  AnalyzerStats analyzer_stats_ RTC_GUARDED_BY(comparison_lock_);

  std::vector<std::unique_ptr<rtc::PlatformThread>> thread_pool_;
  rtc::Event comparison_available_event_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_H_
