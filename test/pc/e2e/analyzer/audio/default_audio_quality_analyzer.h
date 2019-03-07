/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_AUDIO_DEFAULT_AUDIO_QUALITY_ANALYZER_H_
#define TEST_PC_E2E_ANALYZER_AUDIO_DEFAULT_AUDIO_QUALITY_ANALYZER_H_

#include <map>
#include <string>

#include "absl/strings/string_view.h"
#include "api/stats_types.h"
#include "rtc_base/numerics/samples_stats_counter.h"
#include "test/pc/e2e/api/audio_quality_analyzer_interface.h"

namespace webrtc {
namespace test {

struct AudioStreamStats {
 public:
  SamplesStatsCounter expand_rate;
  SamplesStatsCounter accelerate_rate;
  SamplesStatsCounter preemptive_rate;
  SamplesStatsCounter speech_expand_rate;
  SamplesStatsCounter preferred_buffer_size_ms;

  bool IsEmpty() const;
};

class DefaultAudioQualityAnalyzer : public AudioQualityAnalyzerInterface {
 public:
  void Start(std::string test_case_name) override;
  void OnStatsReports(absl::string_view pc_label,
                      const StatsReports& stats_reports) override;
  void Stop() override;

  void SetTrackStreamMap(std::map<std::string, std::string>&) override;

 private:
  const std::string& GetStreamLabelFromStatsReport(
      const StatsReport* stats_report) const;
  AudioStreamStats& GetAudioStreamStats(const std::string& stream_label);
  std::string GetTestCaseName(const std::string& stream_label) const;
  void ReportResult(const std::string& metric_name,
                    const std::string& stream_label,
                    const SamplesStatsCounter& counter,
                    const std::string& unit) const;

  std::string test_case_name_;
  std::map<std::string, std::string> track_stream_map_;
  std::map<std::string, AudioStreamStats> streams_stats_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_AUDIO_DEFAULT_AUDIO_QUALITY_ANALYZER_H_
