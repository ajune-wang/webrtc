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

#include "api/stats/rtc_stats_report.h"
#include "api/stats_types.h"
#include "api/test/audio_quality_analyzer_interface.h"
#include "api/test/track_id_stream_label_map.h"
#include "api/units/timestamp.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/numerics/samples_stats_counter.h"
#include "system_wrappers/include/clock.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace webrtc_pc_e2e {

struct AudioStreamStats {
  SamplesStatsCounter expand_rate;
  SamplesStatsCounter accelerate_rate;
  SamplesStatsCounter preemptive_rate;
  SamplesStatsCounter speech_expand_rate;
  SamplesStatsCounter preferred_buffer_size_ms;

  SamplesStatsCounter expand_rate_new;
  SamplesStatsCounter accelerate_rate_new;
  SamplesStatsCounter preemptive_rate_new;
  SamplesStatsCounter speech_expand_rate_new;
  SamplesStatsCounter preferred_buffer_size_ms_new;
};

// TODO(bugs.webrtc.org/10430): Migrate to the new GetStats as soon as
// bugs.webrtc.org/10428 is fixed.
class DefaultAudioQualityAnalyzer : public AudioQualityAnalyzerInterface {
 public:
  DefaultAudioQualityAnalyzer() : clock_(Clock::GetRealTimeClock()) {}
  ~DefaultAudioQualityAnalyzer() override = default;

  void Start(std::string test_case_name,
             TrackIdStreamLabelMap* analyzer_helper) override;
  void OnStatsReports(const std::string& pc_label,
                      const StatsReports& stats_reports) override;
  void OnStatsReports(
      const std::string& pc_label,
      const rtc::scoped_refptr<const RTCStatsReport>& report) override;
  void Stop() override;

  // Returns audio quality stats per stream label.
  std::map<std::string, AudioStreamStats> GetAudioStreamsStats() const;

 private:
  struct StatsSample {
    uint64_t total_samples_received = 0;
    uint64_t concealed_samples = 0;
    uint64_t removed_samples_for_acceleration = 0;
    uint64_t inserted_samples_for_deceleration = 0;
    uint64_t silent_concealed_samples = 0;
    double jitter_buffer_target_delay = 0;
    uint64_t jitter_buffer_emitted_count = 0;
    int64_t sample_time_us = 0;

    bool IsEmpty() const { return sample_time_us == 0; }

    void DebugLog() {
      std::printf(
          "[%lu] total_samples_received=%lu; concealed_samples=%lu; "
          "removed_samples_for_acceleration=%lu; "
          "inserted_samples_for_deceleration=%lu; "
          "silent_concealed_samples=%lu;\n",
          sample_time_us, total_samples_received, concealed_samples,
          removed_samples_for_acceleration, inserted_samples_for_deceleration,
          silent_concealed_samples);
    }
  };

  const std::string& GetStreamLabelFromStatsReport(
      const StatsReport* stats_report) const;
  std::string GetTestCaseName(const std::string& stream_label) const;
  void ReportResult(const std::string& metric_name,
                    const std::string& stream_label,
                    const SamplesStatsCounter& counter,
                    const std::string& unit,
                    webrtc::test::ImproveDirection improve_direction) const;
  Timestamp Now() const { return clock_->CurrentTime(); }

  Clock* const clock_;
  absl::optional<Timestamp> start_time_;

  std::string test_case_name_;
  TrackIdStreamLabelMap* analyzer_helper_;

  rtc::CriticalSection lock_;
  std::map<std::string, AudioStreamStats> streams_stats_ RTC_GUARDED_BY(lock_);
  std::map<std::string, StatsSample> last_stats_sample_ RTC_GUARDED_BY(lock_);
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_AUDIO_DEFAULT_AUDIO_QUALITY_ANALYZER_H_
