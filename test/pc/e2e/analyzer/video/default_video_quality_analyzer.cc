/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"

#include <algorithm>
#include <utility>

#include "absl/memory/memory.h"
#include "api/units/time_delta.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/logging.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace test {
namespace {

constexpr int kMaxActiveComparisons = 10;
constexpr uint64_t kFreezeThresholdMs = 150;

}  // namespace

DefaultVideoQualityAnalyzer::DefaultVideoQualityAnalyzer(std::string test_label)
    : test_label_(std::move(test_label)), clock_(Clock::GetRealTimeClock()) {}
DefaultVideoQualityAnalyzer::~DefaultVideoQualityAnalyzer() {
  Stop();
}

void DefaultVideoQualityAnalyzer::Start(int max_threads_count) {
  for (int i = 0; i < max_threads_count; i++) {
    auto thread = absl::make_unique<rtc::PlatformThread>(
        &DefaultVideoQualityAnalyzer::ProcessComparisonsThread, this,
        ("DefaultVideoQualityAnalyzerWorker-" + std::to_string(i)).data(),
        rtc::ThreadPriority::kNormalPriority);
    thread->Start();
    thread_pool_.push_back(std::move(thread));
  }
  {
    rtc::CritScope crit(&lock_);
    state_ = State::kActive;
  }
}

uint16_t DefaultVideoQualityAnalyzer::OnFrameCaptured(
    const std::string& stream_label,
    const webrtc::VideoFrame& frame) {
  uint16_t frame_id = next_frame_id_++;
  {
    // Ensure stats for this stream exists.
    rtc::CritScope crit(&comparison_lock_);
    if (stream_stats_.find(stream_label) == stream_stats_.end()) {
      stream_stats_.insert({stream_label, StreamStats()});
      // Assume that the first freeze was before first stream frame captured
      stream_last_freeze_end_time_.insert({stream_label, Now()});
    }
  }
  {
    rtc::CritScope crit(&lock_);
    frame_counters_.captured++;
    stream_frame_counters_[stream_label].captured++;

    stream_frame_id_list_[stream_label].push_back(frame_id);
    // Update frames in flight info.
    auto it = captured_frames_in_flight_.find(frame_id);
    if (it != captured_frames_in_flight_.end()) {
      // We overflow uint16_t and hit previous frame id and this frame is still
      // in flight. So it is counted as dropped already. We won't send this
      // frame to comparison, because it is very late detection and the frame
      // was already processed by sampling comparison.
      auto stats_it = frame_stats_.find(frame_id);
      RTC_DCHECK(stats_it != frame_stats_.end());
      captured_frames_in_flight_.erase(it);
      frame_stats_.erase(stats_it);
    }
    captured_frames_in_flight_.insert(
        std::pair<uint16_t, VideoFrame>(frame_id, frame));
    // Set frame id on local copy of the frame
    captured_frames_in_flight_.at(frame_id).set_id(frame_id);
    frame_stats_.insert(std::pair<uint16_t, FrameStats>(
        frame_id, FrameStats(stream_label, /*captured_time=*/Now())));
  }
  return frame_id;
}

void DefaultVideoQualityAnalyzer::OnFramePreEncode(
    const webrtc::VideoFrame& frame) {
  rtc::CritScope crit(&lock_);
  auto it = frame_stats_.find(frame.id());
  RTC_DCHECK(it != frame_stats_.end());
  frame_counters_.pre_encoded++;
  stream_frame_counters_[it->second.stream_label].pre_encoded++;
  it->second.pre_encode_time = Now();
}

void DefaultVideoQualityAnalyzer::OnFrameEncoded(
    uint16_t frame_id,
    const webrtc::EncodedImage& encoded_image) {
  rtc::CritScope crit(&lock_);
  // TODO(titovartem) we need to pick right spatial index here
  // We have several options:
  //  1. Have per stream config with spatial index for each stream
  //     Pros:
  //      * Set of streams defined before, so we can precreate empty stats
  //      * Fully specified behavior
  //     Cons:
  //      * Can't support unspecified stream and will fail if will meet them
  //      * Extra configuration
  //  2. Have global default config as addon to 1.
  //     Pros: Support unknown streams
  //     Cons: quite a lot of configs...
  //  3. Have single global config:
  //     Pros:
  //      * No need to predefine streams
  //      * Easy to configure
  //     Cons:
  //      * Impossible to have one stream with spatial layers and one without.
  auto it = frame_stats_.find(frame_id);
  RTC_DCHECK(it != frame_stats_.end());
  RTC_DCHECK(it->second.encoded_time.IsInfinite())
      << "Received multiple spatial layers for stream "
      << it->second.stream_label;
  frame_counters_.encoded++;
  stream_frame_counters_[it->second.stream_label].encoded++;
  it->second.encoded_time = Now();
  it->second.encoded_frame_size = encoded_image.size();
}

void DefaultVideoQualityAnalyzer::OnFrameDropped(
    webrtc::EncodedImageCallback::DropReason reason) {
  // Here we do nothing, because we will see this drop on renderer side.
}

void DefaultVideoQualityAnalyzer::OnFrameReceived(
    uint16_t frame_id,
    const webrtc::EncodedImage& input_image) {
  // TODO(titovartem) We should always receive only single spatial layer here,
  // so need to apply filtering on incoming streams:
  // For VP9 (SVC) - remove all spatial layers above required and pass to
  // decoder
  // For simulcast - decode only required and produce black image for others.
  // Spatial layer index should be added to frame id injector
  rtc::CritScope crit(&lock_);
  auto it = frame_stats_.find(frame_id);
  RTC_DCHECK(it != frame_stats_.end());
  RTC_DCHECK(it->second.received_time.IsInfinite())
      << "Received multiple spatial layers for stream "
      << it->second.stream_label;
  frame_counters_.received++;
  stream_frame_counters_[it->second.stream_label].received++;
  it->second.received_time = Now();
}

void DefaultVideoQualityAnalyzer::OnFrameDecoded(
    const webrtc::VideoFrame& frame,
    absl::optional<int32_t> decode_time_ms,
    absl::optional<uint8_t> qp) {
  rtc::CritScope crit(&lock_);
  auto it = frame_stats_.find(frame.id());
  RTC_DCHECK(it != frame_stats_.end());
  frame_counters_.decoded++;
  stream_frame_counters_[it->second.stream_label].decoded++;
  it->second.decoded_time = Now();
  it->second.decoder_reported_time_ms = decode_time_ms;
  it->second.decoder_reported_qp = qp;
}

void DefaultVideoQualityAnalyzer::OnFrameRendered(
    const webrtc::VideoFrame& frame) {
  rtc::CritScope crit(&lock_);
  auto stats_it = frame_stats_.find(frame.id());
  RTC_DCHECK(stats_it != frame_stats_.end());
  FrameStats* frame_stats = &stats_it->second;
  // Update frames counters
  frame_counters_.rendered++;
  stream_frame_counters_[frame_stats->stream_label].rendered++;

  // Update current frame stats
  frame_stats->rendered_time = Now();
  frame_stats->rendered_frame_width = frame.width();
  frame_stats->rendered_frame_height = frame.height();

  // Find corresponding captured frame
  auto frame_it = captured_frames_in_flight_.find(frame.id());
  RTC_DCHECK(frame_it != captured_frames_in_flight_.end());
  const VideoFrame& captured_frame = frame_it->second;

  // After we received frame here we need to check are there any dropped frames
  // between this one and last one, that was received for this video stream.

  const std::string& stream_label = frame_stats->stream_label;
  VideoFrame* last_rendered_frame = GetLastRenderedFrame(stream_label);
  std::list<uint16_t>& stream_frame_ids = stream_frame_id_list_[stream_label];
  int dropped_count = 0;
  while (!stream_frame_ids.empty() && stream_frame_ids.front() != frame.id()) {
    dropped_count++;
    uint16_t dropped_frame_id = stream_frame_ids.front();
    stream_frame_ids.pop_front();
    // Frame with id |dropped_frame_id| was dropped. We need:
    // 1. Update global and stream frame counters
    // 2. Extract corresponding frame from |captured_frames_in_flight_|
    // 3. Extract corresponding frame stats from |frame_stats_|
    // 4. Send extracted frame to comparison with dropped=true
    // 5. Cleanup dropped frame
    frame_counters_.dropped++;
    stream_frame_counters_[stream_label].dropped++;

    auto dropped_frame_stats_it = frame_stats_.find(dropped_frame_id);
    RTC_DCHECK(dropped_frame_stats_it != frame_stats_.end());
    auto dropped_frame_it = captured_frames_in_flight_.find(dropped_frame_id);
    RTC_CHECK(dropped_frame_it != captured_frames_in_flight_.end());

    AddComparison(dropped_frame_it->second,
                  last_rendered_frame == nullptr
                      ? absl::nullopt
                      : absl::make_optional(*last_rendered_frame),
                  true, dropped_frame_stats_it->second);

    frame_stats_.erase(dropped_frame_stats_it);
    captured_frames_in_flight_.erase(dropped_frame_it);
  }
  RTC_DCHECK(!stream_frame_ids.empty());
  stream_frame_ids.pop_front();

  SetLastRenderedFrame(stream_label, frame);
  auto time_it = stream_last_rendered_frame_time_.find(stream_label);
  if (time_it != stream_last_rendered_frame_time_.end()) {
    frame_stats->prev_frame_rendered_time = time_it->second;
    time_it->second = frame_stats->rendered_time;
  } else {
    stream_last_rendered_frame_time_.insert(
        {stream_label, frame_stats->rendered_time});
  }
  {
    rtc::CritScope cr(&comparison_lock_);
    stream_stats_[stream_label].skipped_between_rendered.AddSample(
        dropped_count);
  }
  AddComparison(captured_frame, frame, false, *frame_stats);

  captured_frames_in_flight_.erase(frame_it);
  frame_stats_.erase(stats_it);
}

VideoFrame* DefaultVideoQualityAnalyzer::GetLastRenderedFrame(
    const std::string& stream_label) {
  auto it = stream_last_rendered_frame_.find(stream_label);
  if (it == stream_last_rendered_frame_.end()) {
    return nullptr;
  }
  return &it->second;
}

void DefaultVideoQualityAnalyzer::SetLastRenderedFrame(
    const std::string& stream_label,
    const VideoFrame& frame) {
  auto it = stream_last_rendered_frame_.find(stream_label);
  if (it == stream_last_rendered_frame_.end()) {
    stream_last_rendered_frame_.insert({stream_label, frame});
  } else {
    it->second = frame;
  }
}

void DefaultVideoQualityAnalyzer::OnEncoderError(
    const webrtc::VideoFrame& frame,
    int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Encoder error for frame [" << frame.id()
                    << "]. Code: " << error_code;
}

void DefaultVideoQualityAnalyzer::OnDecoderError(uint16_t frame_id,
                                                 int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Decoder error for frame [" << frame_id
                    << "]. Code: " << error_code;
}

void DefaultVideoQualityAnalyzer::Stop() {
  {
    rtc::CritScope crit(&lock_);
    if (state_ == State::kStopped) {
      return;
    }
    state_ = State::kStopped;
  }
  comparison_available_event_.Set();
  for (auto& thread : thread_pool_) {
    thread->Stop();
  }
  // PlatformThread have to be deleted on the same thread, where it was created
  thread_pool_.clear();

  // Perform final Metrics update. On this place analyzer is stopped and no one
  // holds any locks.
  {
    // 1. Time between freezes
    //    Assume that after call there is a freeze, if there was at least one
    //    real freeze during the call.
    rtc::CritScope crit1(&lock_);
    rtc::CritScope crit2(&comparison_lock_);
    for (auto& item : stream_stats_) {
      if (item.second.freeze_time_ms.IsEmpty()) {
        continue;
      }
      item.second.time_between_freezes_ms.AddSample(
          (stream_last_rendered_frame_time_.at(item.first) -
           stream_last_freeze_end_time_.at(item.first))
              .ms());
    }
  }
}

std::set<std::string> DefaultVideoQualityAnalyzer::GetKnownVideoStreams()
    const {
  rtc::CritScope crit2(&comparison_lock_);
  std::set<std::string> out;
  for (auto& item : stream_stats_) {
    out.insert(item.first);
  }
  return out;
}

void DefaultVideoQualityAnalyzer::ReportResults(
    const std::string& stream_label) const {
  rtc::CritScope crit1(&lock_);
  rtc::CritScope crit2(&comparison_lock_);
  auto it = stream_stats_.find(stream_label);
  RTC_CHECK(it != stream_stats_.end());
  const StreamStats& stats = it->second;
  ReportResult("psnr", stream_label, stats.psnr, "dB");
  ReportResult("ssim", stream_label, stats.psnr, "unitless");
  ReportResult("transport_time", stream_label, stats.transport_time_ms, "ms");
  ReportResult("total_delay_incl_transport", stream_label,
               stats.total_delay_incl_transport_ms, "ms");
  ReportResult("time_between_rendered_frames", stream_label,
               stats.time_between_rendered_frames_ms, "ms");
  test::PrintResult("encode_frame_rate", "", GetTraceName(stream_label),
                    stats.encode_frame_rate.IsEmpty()
                        ? 0
                        : stats.encode_frame_rate.GetEventsPerSecond(),
                    "unitless", false);
  ReportResult("encode_time", stream_label, stats.encode_time_ms, "ms");
  ReportResult("time_between_freezes", stream_label,
               stats.time_between_freezes_ms, "ms");
  ReportResult("pixels_per_frame", stream_label, stats.encoded_pix, "unitless");
  test::PrintResult("min_psnr", "", GetTraceName(stream_label),
                    stats.psnr.IsEmpty() ? 0 : stats.psnr.GetMin(), "unitless",
                    false);
  ReportResult("decode_time", stream_label, stats.decode_time_ms, "ms");
  test::PrintResult("dropped_frames", "", GetTraceName(stream_label),
                    stream_frame_counters_.at(stream_label).dropped, "unitless",
                    false);
  test::PrintResult("freeze_200ms", "", GetTraceName(stream_label),
                    stats.freeze_200_ms, "ms", false);
  test::PrintResult("freeze_1s", "", GetTraceName(stream_label),
                    stats.freeze_1s, "ms", false);
  ReportResult("max_skipped", stream_label, stats.skipped_between_rendered,
               "unitless");
}

FrameCounters DefaultVideoQualityAnalyzer::GetGlobalCounters() {
  rtc::CritScope crit(&lock_);
  return frame_counters_;
}

std::map<std::string, FrameCounters>
DefaultVideoQualityAnalyzer::GetPerStreamCounters() const {
  rtc::CritScope crit(&lock_);
  return stream_frame_counters_;
}

std::map<std::string, StreamStats> DefaultVideoQualityAnalyzer::GetStats()
    const {
  rtc::CritScope cri(&comparison_lock_);
  return stream_stats_;
};

AnalyzerStats DefaultVideoQualityAnalyzer::GetAnalyzerStats() const {
  rtc::CritScope crit(&comparison_lock_);
  return analyzer_stats_;
}

void DefaultVideoQualityAnalyzer::AddComparison(
    absl::optional<VideoFrame> captured,
    absl::optional<VideoFrame> rendered,
    bool dropped,
    FrameStats frame_stats) {
  rtc::CritScope crit(&comparison_lock_);
  analyzer_stats_.comparisons_queue_size.AddSample(comparisons_.size());
  // If there too many computations waiting in the queue, we won't provide
  // frames itself to make future computations lighter.
  if (comparisons_.size() >= kMaxActiveComparisons) {
    comparisons_.emplace_back(dropped, frame_stats);
  } else {
    comparisons_.emplace_back(std::move(captured), std::move(rendered), dropped,
                              frame_stats);
  }
  comparison_available_event_.Set();
}

void DefaultVideoQualityAnalyzer::ProcessComparisonsThread(void* obj) {
  static_cast<DefaultVideoQualityAnalyzer*>(obj)->ProcessComparisons();
}

void DefaultVideoQualityAnalyzer::ProcessComparisons() {
  while (true) {
    // Try to pick next comparison to perform from the queue.
    absl::optional<FrameComparison> comparison = absl::nullopt;
    {
      rtc::CritScope crit(&comparison_lock_);
      if (!comparisons_.empty()) {
        comparison = comparisons_.front();
        comparisons_.pop_front();
      }
    }
    if (!comparison) {
      bool more_frames_expected;
      {
        // If there are no comparisons and state is stopped =>
        // no more frames expected.
        rtc::CritScope crit(&lock_);
        more_frames_expected = state_ != State::kStopped;
      }
      if (!more_frames_expected) {
        comparison_available_event_.Set();
        return;
      }
      comparison_available_event_.Wait(1000);
      continue;
    }

    ProcessComparison(comparison.value());
  }
}

void DefaultVideoQualityAnalyzer::ProcessComparison(
    const FrameComparison& comparison) {
  // Perform expensive psnr and ssim calculations while not holding lock.
  double psnr = -1.0;
  double ssim = -1.0;
  if (comparison.captured && !comparison.dropped) {
    psnr = I420PSNR(&*comparison.captured, &*comparison.rendered);
    ssim = I420SSIM(&*comparison.captured, &*comparison.rendered);
  }

  const FrameStats& frame_stats = comparison.frame_stats;

  rtc::CritScope crit(&comparison_lock_);
  auto stats_it = stream_stats_.find(frame_stats.stream_label);
  RTC_CHECK(stats_it != stream_stats_.end());
  StreamStats* stats = &stats_it->second;
  stats->comparisons_done++;
  if (!comparison.captured) {
    stats->overloaded_comparisons_done++;
  }
  if (psnr > 0) {
    stats->psnr.AddSample(psnr);
  }
  if (ssim > 0) {
    stats->ssim.AddSample(ssim);
  }
  if (frame_stats.encoded_time.IsFinite()) {
    stats->encode_time_ms.AddSample(
        (frame_stats.encoded_time - frame_stats.pre_encode_time).ms());
    stats->encode_frame_rate.AddEvent(frame_stats.encoded_time);
  } else {
    if (frame_stats.pre_encode_time.IsFinite()) {
      stats->dropped_by_encoder++;
    } else {
      stats->dropped_before_encoder++;
    }
  }
  if (!comparison.dropped) {
    // This stats can be calculated only if frame was received on remote side.
    if (comparison.frame_stats.rendered_frame_width &&
        comparison.frame_stats.rendered_frame_height) {
      stats->encoded_pix.AddSample(
          *comparison.frame_stats.rendered_frame_width *
          *comparison.frame_stats.rendered_frame_height);
    }
    stats->transport_time_ms.AddSample(
        (frame_stats.received_time - frame_stats.encoded_time).ms());
    stats->total_delay_incl_transport_ms.AddSample(
        (frame_stats.rendered_time - frame_stats.captured_time).ms());
    // TODO(titovartem) Should we use
    // |comparison.frame_stats.decoder_reported_time_ms| here?
    stats->decode_time_ms.AddSample(
        (frame_stats.decoded_time - frame_stats.received_time).ms());

    if (frame_stats.prev_frame_rendered_time.IsFinite()) {
      // This stats can be calculated only if there was previous rendered frame.
      TimeDelta time_between_rendered_frames =
          frame_stats.rendered_time - frame_stats.prev_frame_rendered_time;
      stats->time_between_rendered_frames_ms.AddSample(
          time_between_rendered_frames.ms());
      double average_time_between_rendered_frames_ms =
          stats->time_between_rendered_frames_ms.GetAverage();
      if (time_between_rendered_frames.ms() >
          std::max(kFreezeThresholdMs + average_time_between_rendered_frames_ms,
                   3 * average_time_between_rendered_frames_ms)) {
        stats->freeze_time_ms.AddSample(time_between_rendered_frames.ms());
        auto freeze_end_it =
            stream_last_freeze_end_time_.find(frame_stats.stream_label);
        if (freeze_end_it != stream_last_freeze_end_time_.end()) {
          stats->time_between_freezes_ms.AddSample(
              (frame_stats.prev_frame_rendered_time - freeze_end_it->second)
                  .ms());
          freeze_end_it->second = frame_stats.rendered_time;
        } else {
          stream_last_freeze_end_time_.emplace(frame_stats.stream_label,
                                               frame_stats.rendered_time);
        }
      }
      if (time_between_rendered_frames.ms() >= 200) {
        stats->freeze_200_ms += time_between_rendered_frames.ms();
      }
      if (time_between_rendered_frames.ms() >= 1000) {
        stats->freeze_1s += time_between_rendered_frames.ms();
      }
    }
  }
}

void DefaultVideoQualityAnalyzer::ReportResult(
    const std::string& metric_name,
    const std::string& stream_label,
    const SamplesStatsCounter& counter,
    const std::string& unit) const {
  test::PrintResultMeanAndError(
      metric_name, "", GetTraceName(stream_label),
      counter.IsEmpty() ? 0 : counter.GetAverage(),
      counter.IsEmpty() ? 0 : counter.GetStandardDeviation(), unit, false);
}

std::string DefaultVideoQualityAnalyzer::GetTraceName(
    const std::string& stream_label) const {
  return test_label_ + "_" + stream_label;
}

Timestamp DefaultVideoQualityAnalyzer::Now() {
  return Timestamp::us(clock_->TimeInMicroseconds());
}

DefaultVideoQualityAnalyzer::FrameStats::FrameStats(std::string stream_label,
                                                    Timestamp captured_time)
    : stream_label(std::move(stream_label)), captured_time(captured_time) {}

DefaultVideoQualityAnalyzer::FrameComparison::FrameComparison(
    absl::optional<VideoFrame> captured,
    absl::optional<VideoFrame> rendered,
    bool dropped,
    FrameStats frame_stats)
    : captured(std::move(captured)),
      rendered(std::move(rendered)),
      dropped(dropped),
      frame_stats(std::move(frame_stats)) {}

DefaultVideoQualityAnalyzer::FrameComparison::FrameComparison(
    bool dropped,
    FrameStats frame_stats)
    : captured(absl::nullopt),
      rendered(absl::nullopt),
      dropped(dropped),
      frame_stats(std::move(frame_stats)) {}

}  // namespace test
}  // namespace webrtc
