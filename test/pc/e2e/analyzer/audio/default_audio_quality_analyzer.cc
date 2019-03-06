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

#include <string.h>

#include "api/stats_types.h"
#include "rtc_base/logging.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace test {

bool AudioStreamStats::IsEmpty() const {
  return expand_rate.IsEmpty() && accelerate_rate.IsEmpty() &&
         preemptive_rate.IsEmpty() && speech_expand_rate.IsEmpty() &&
         preferred_buffer_size_ms.IsEmpty();
}

void DefaultAudioQualityAnalyzer::Start(std::string test_case_name) {
  test_case_name_ = std::move(test_case_name);
}

// TODO(mbonadei): pc_label is not useful anymore, the analyzer will work on
// the concept of stream.
void DefaultAudioQualityAnalyzer::OnStatsReports(
    absl::string_view pc_label,
    const StatsReports& stats_reports) {
  for (auto* stats_report : stats_reports) {
    // NetEq stats are only present in kStatsReportTypeSsrc reports, so all
    // other reports are just ignored.
    if (stats_report->type() != StatsReport::StatsType::kStatsReportTypeSsrc) {
      continue;
    }
    // Ignoring stats reports of "video" SSRC.
    auto* value_media_type = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNameMediaType);
    RTC_CHECK(value_media_type);
    if (strcmp(value_media_type->static_string_val(), "audio") != 0) {
      continue;
    }
    auto* packets_received = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNamePacketsReceived);
    // TODO(mbonadei): This could be a problem in case no packets are received
    // during a call. We want to measure stats also in that case, so the
    // check == 0 should be replaced with something else.
    if (!packets_received || packets_received->int_val() == 0) {
      // Discarding stats about send-side SSRC since NetEq stats are only
      // available in recv-side SSRC.
      continue;
    }

    std::string& stream_label = GetStreamLabelFromStatsReport(stats_report);
    AudioStreamStats& audio_stream_stats = GetAudioStreamStats(stream_label);
    RTC_LOG(INFO) << stream_label;
    auto* expand_rate = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNameExpandRate);
    auto* accelerate_rate = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNameAccelerateRate);
    auto* preemptive_rate = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNamePreemptiveExpandRate);
    auto* speech_expand_rate = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNameSpeechExpandRate);
    auto* preferred_buffer_size_ms = stats_report->FindValue(
        StatsReport::StatsValueName::kStatsValueNamePreferredJitterBufferMs);
    RTC_CHECK(expand_rate);
    RTC_CHECK(accelerate_rate);
    RTC_CHECK(preemptive_rate);
    RTC_CHECK(speech_expand_rate);
    RTC_CHECK(preferred_buffer_size_ms);
    audio_stream_stats.expand_rate.AddSample(expand_rate->float_val());
    audio_stream_stats.accelerate_rate.AddSample(accelerate_rate->float_val());
    audio_stream_stats.preemptive_rate.AddSample(preemptive_rate->float_val());
    audio_stream_stats.speech_expand_rate.AddSample(
        speech_expand_rate->float_val());
    audio_stream_stats.preferred_buffer_size_ms.AddSample(
        preferred_buffer_size_ms->int_val());
  }
}

std::string& DefaultAudioQualityAnalyzer::GetStreamLabelFromStatsReport(
    const StatsReport* stats_report) {
  auto* value_track_id = stats_report->FindValue(
      StatsReport::StatsValueName::kStatsValueNameTrackId);
  RTC_CHECK(value_track_id);
  auto stream_id_pair = track_stream_map_.find(value_track_id->string_val());
  RTC_CHECK(stream_id_pair != track_stream_map_.end());
  return stream_id_pair->second;
}

AudioStreamStats& DefaultAudioQualityAnalyzer::GetAudioStreamStats(
    std::string& stream_label) {
  auto stream_stats = streams_stats_.find(stream_label);
  RTC_CHECK(stream_stats != streams_stats_.end());
  return stream_stats->second;
}

std::string DefaultAudioQualityAnalyzer::GetTestCaseName(
    const std::string& stream_label) {
  return test_case_name_ + "/" + stream_label;
}

void DefaultAudioQualityAnalyzer::Stop() {
  for (auto& item : streams_stats_) {
    if (item.second.IsEmpty()) {
      // TODO(mbonadei): change the API of the audio analyzer in order to
      // explicitly set the streams we need to track.
      // This is a prototype only workaround.
      continue;
    }
    test::PrintResultMeanAndError(
        "expand_rate", /*modifier=*/"", GetTestCaseName(item.first),
        item.second.expand_rate.IsEmpty()
            ? 0
            : item.second.expand_rate.GetAverage(),
        item.second.expand_rate.IsEmpty()
            ? 0
            : item.second.expand_rate.GetStandardDeviation(),
        "unitless", /*important=*/false);
    test::PrintResultMeanAndError(
        "accelerate_rate", /*modifier=*/"", GetTestCaseName(item.first),
        item.second.accelerate_rate.IsEmpty()
            ? 0
            : item.second.accelerate_rate.GetAverage(),
        item.second.accelerate_rate.IsEmpty()
            ? 0
            : item.second.accelerate_rate.GetStandardDeviation(),
        "unitless", /*important=*/false);
    test::PrintResultMeanAndError(
        "preemptive_rate", /*modifier=*/"", GetTestCaseName(item.first),
        item.second.preemptive_rate.IsEmpty()
            ? 0
            : item.second.preemptive_rate.GetAverage(),
        item.second.preemptive_rate.IsEmpty()
            ? 0
            : item.second.preemptive_rate.GetStandardDeviation(),
        "unitless", /*important=*/false);
    test::PrintResultMeanAndError(
        "speech_expand_rate", /*modifier=*/"", GetTestCaseName(item.first),
        item.second.speech_expand_rate.IsEmpty()
            ? 0
            : item.second.speech_expand_rate.GetAverage(),
        item.second.speech_expand_rate.IsEmpty()
            ? 0
            : item.second.speech_expand_rate.GetStandardDeviation(),
        "unitless", /*important=*/false);
    test::PrintResultMeanAndError(
        "preferred_buffer_size_ms", /*modifier=*/"",
        GetTestCaseName(item.first),
        item.second.preferred_buffer_size_ms.IsEmpty()
            ? 0
            : item.second.preferred_buffer_size_ms.GetAverage(),
        item.second.preferred_buffer_size_ms.IsEmpty()
            ? 0
            : item.second.preferred_buffer_size_ms.GetStandardDeviation(),
        "unitless", /*important=*/false);
  }
}

void DefaultAudioQualityAnalyzer::SetTrackStreamMap(
    std::map<std::string, std::string>& map) {
  track_stream_map_ = map;
  for (auto& track_to_stream : track_stream_map_) {
    streams_stats_.insert({track_to_stream.second, AudioStreamStats()});
  }
}

}  // namespace test
}  // namespace webrtc
