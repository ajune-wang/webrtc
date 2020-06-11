/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/audio/default_audio_quality_analyzer.h"

#include "api/stats/rtcstats_objects.h"
#include "api/stats_types.h"
#include "api/units/timestamp.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace webrtc_pc_e2e {

void DefaultAudioQualityAnalyzer::Start(
    std::string test_case_name,
    TrackIdStreamLabelMap* analyzer_helper) {
  test_case_name_ = std::move(test_case_name);
  analyzer_helper_ = analyzer_helper;
}

void DefaultAudioQualityAnalyzer::OnStatsReports(
    const std::string& pc_label,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  // TODO(landrey): use "inbound-rtp" instead of "track" stats when required
  // audio metrics moved there
  auto stats = report->GetStatsOfType<RTCMediaStreamTrackStats>();

  for (auto& stat : stats) {
    if (!stat->kind.is_defined() ||
        !(*stat->kind == RTCMediaStreamTrackKind::kAudio) ||
        !*stat->remote_source) {
      continue;
    }

    StatsSample sample;
    if (stat->total_samples_received.is_defined()) {
      sample.total_samples_received = *stat->total_samples_received;
    }
    if (stat->concealed_samples.is_defined()) {
      sample.concealed_samples = *stat->concealed_samples;
    }
    if (stat->removed_samples_for_acceleration.is_defined()) {
      sample.removed_samples_for_acceleration =
          *stat->removed_samples_for_acceleration;
    }
    if (stat->inserted_samples_for_deceleration.is_defined()) {
      sample.inserted_samples_for_deceleration =
          *stat->inserted_samples_for_deceleration;
    }
    if (stat->silent_concealed_samples.is_defined()) {
      sample.silent_concealed_samples = *stat->silent_concealed_samples;
    }
    if (stat->jitter_buffer_target_delay.is_defined()) {
      sample.jitter_buffer_target_delay = *stat->jitter_buffer_target_delay;
    }
    if (stat->jitter_buffer_emitted_count.is_defined()) {
      sample.jitter_buffer_emitted_count = *stat->jitter_buffer_emitted_count;
    }

    const std::string& stream_label =
        analyzer_helper_->GetStreamLabelFromTrackId(*stat->track_identifier);

    rtc::CritScope crit(&lock_);
    StatsSample prev_sample = last_stats_sample_[stream_label];
    double total_samples_diff = static_cast<double>(
        sample.total_samples_received - prev_sample.total_samples_received);
    if (total_samples_diff == 0) {
      return;
    }

    AudioStreamStats& audio_stream_stats = streams_stats_[stream_label];
    audio_stream_stats.expand_rate.AddSample(
        (sample.concealed_samples - prev_sample.concealed_samples) /
        total_samples_diff);
    audio_stream_stats.accelerate_rate.AddSample(
        (sample.removed_samples_for_acceleration -
         prev_sample.removed_samples_for_acceleration) /
        total_samples_diff);
    audio_stream_stats.preemptive_rate.AddSample(
        (sample.inserted_samples_for_deceleration -
         prev_sample.inserted_samples_for_deceleration) /
        total_samples_diff);

    int64_t speech_concealed_samples =
        sample.concealed_samples - sample.silent_concealed_samples;
    int64_t prev_speech_concealed_samples =
        prev_sample.concealed_samples - prev_sample.silent_concealed_samples;
    audio_stream_stats.speech_expand_rate.AddSample(
        (speech_concealed_samples - prev_speech_concealed_samples) /
        total_samples_diff);

    int64_t jitter_buffer_emitted_count_diff =
        sample.jitter_buffer_emitted_count -
        prev_sample.jitter_buffer_emitted_count;
    if (jitter_buffer_emitted_count_diff > 0) {
      double jitter_buffer_target_delay_diff =
          sample.jitter_buffer_target_delay -
          prev_sample.jitter_buffer_target_delay;
      double jitter_buffer_target_delay_diff_ms =
          Timestamp::Seconds(jitter_buffer_target_delay_diff).ms<double>();
      audio_stream_stats.preferred_buffer_size_ms.AddSample(
          jitter_buffer_target_delay_diff_ms /
          jitter_buffer_emitted_count_diff);
    }

    last_stats_sample_[stream_label] = sample;
  }
}

const std::string& DefaultAudioQualityAnalyzer::GetStreamLabelFromStatsReport(
    const StatsReport* stats_report) const {
  const webrtc::StatsReport::Value* report_track_id = stats_report->FindValue(
      StatsReport::StatsValueName::kStatsValueNameTrackId);
  RTC_CHECK(report_track_id);
  return analyzer_helper_->GetStreamLabelFromTrackId(
      report_track_id->string_val());
}

std::string DefaultAudioQualityAnalyzer::GetTestCaseName(
    const std::string& stream_label) const {
  return test_case_name_ + "/" + stream_label;
}

void DefaultAudioQualityAnalyzer::Stop() {
  using ::webrtc::test::ImproveDirection;
  rtc::CritScope crit(&lock_);
  for (auto& item : streams_stats_) {
    ReportResult("expand_rate", item.first, item.second.expand_rate, "unitless",
                 ImproveDirection::kSmallerIsBetter);
    ReportResult("accelerate_rate", item.first, item.second.accelerate_rate,
                 "unitless", ImproveDirection::kSmallerIsBetter);
    ReportResult("preemptive_rate", item.first, item.second.preemptive_rate,
                 "unitless", ImproveDirection::kSmallerIsBetter);
    ReportResult("speech_expand_rate", item.first,
                 item.second.speech_expand_rate, "unitless",
                 ImproveDirection::kSmallerIsBetter);
    ReportResult("preferred_buffer_size_ms", item.first,
                 item.second.preferred_buffer_size_ms, "ms",
                 ImproveDirection::kNone);
  }
}

std::map<std::string, AudioStreamStats>
DefaultAudioQualityAnalyzer::GetAudioStreamsStats() const {
  rtc::CritScope crit(&lock_);
  return streams_stats_;
}

void DefaultAudioQualityAnalyzer::ReportResult(
    const std::string& metric_name,
    const std::string& stream_label,
    const SamplesStatsCounter& counter,
    const std::string& unit,
    webrtc::test::ImproveDirection improve_direction) const {
  test::PrintResultMeanAndError(
      metric_name, /*modifier=*/"", GetTestCaseName(stream_label),
      counter.IsEmpty() ? 0 : counter.GetAverage(),
      counter.IsEmpty() ? 0 : counter.GetStandardDeviation(), unit,
      /*important=*/false, improve_direction);
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
