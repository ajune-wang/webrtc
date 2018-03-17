/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_EVENT_LOG_VISUALIZER_ANALYZER_H_
#define RTC_TOOLS_EVENT_LOG_VISUALIZER_ANALYZER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_tools/event_log_visualizer/plot_base.h"
#include "rtc_tools/event_log_visualizer/triage_notifications.h"

namespace webrtc {
namespace rtceventlog {

class EventLogAnalyzer {
 public:
  // The EventLogAnalyzer keeps a reference to the ParsedRtcEventLog for the
  // duration of its lifetime. The ParsedRtcEventLog must not be destroyed or
  // modified while the EventLogAnalyzer is being used.
  explicit EventLogAnalyzer(const ParsedRtcEventLog& log);

  template <typename Direction>
  void CreatePacketGraph(Plot* plot);

  template <typename Direction>
  void CreateAccumulatedPacketsGraph(Plot* plot);

  void CreatePlayoutGraph(Plot* plot);

  template <typename Direction>
  void CreateAudioLevelGraph(Plot* plot);

  void CreateSequenceNumberGraph(Plot* plot);

  void CreateIncomingPacketLossGraph(Plot* plot);

  void CreateIncomingDelayDeltaGraph(Plot* plot);
  void CreateIncomingDelayGraph(Plot* plot);

  void CreateFractionLossGraph(Plot* plot);

  void CreateTotalIncomingBitrateGraph(Plot* plot);
  void CreateTotalOutgoingBitrateGraph(Plot* plot,
                                       bool show_detector_state = false,
                                       bool show_alr_state = false);

  template <typename Direction>
  void CreateStreamBitrateGraph(Plot* plot);

  void CreateSendSideBweSimulationGraph(Plot* plot);
  void CreateReceiveSideBweSimulationGraph(Plot* plot);

  void CreateNetworkDelayFeedbackGraph(Plot* plot);
  void CreatePacerDelayGraph(Plot* plot);

  template <typename Direction>
  void CreateTimestampGraph(Plot* plot);

  void CreateAudioEncoderTargetBitrateGraph(Plot* plot);
  void CreateAudioEncoderFrameLengthGraph(Plot* plot);
  void CreateAudioEncoderPacketLossGraph(Plot* plot);
  void CreateAudioEncoderEnableFecGraph(Plot* plot);
  void CreateAudioEncoderEnableDtxGraph(Plot* plot);
  void CreateAudioEncoderNumChannelsGraph(Plot* plot);
  void CreateAudioJitterBufferGraph(const std::string& replacement_file_name,
                                    int file_sample_rate_hz,
                                    Plot* plot);

  void CreateIceCandidatePairConfigGraph(Plot* plot);
  void CreateIceConnectivityCheckGraph(Plot* plot);

  void CreateTriageNotifications();
  void PrintNotifications(FILE* file);

 private:
  template <typename Direction>
  bool IsRtxSsrc(uint32_t ssrc) const {
    static_assert(sizeof(Direction) != sizeof(Direction),
                  "Template argument must be either Incoming or Outgoing");
  }

  template <typename Direction>
  bool IsVideoSsrc(uint32_t ssrc) const {
    static_assert(sizeof(Direction) != sizeof(Direction),
                  "Template argument must be either Incoming or Outgoing");
  }

  template <typename Direction>
  bool IsAudioSsrc(uint32_t ssrc) const {
    static_assert(sizeof(Direction) != sizeof(Direction),
                  "Template argument must be either Incoming or Outgoing");
  }

  template <typename T>
  void CreateAccumulatedPacketsTimeSeries(Plot* plot,
                                          const std::vector<T>& packets,
                                          const std::string& label);

  template <typename Direction>
  void CreateStreamGapNotifications();

  template <typename Direction>
  void CreateTransmissionGapNotifications();

  template <typename Direction>
  std::string GetStreamName(uint32_t ssrc) const {
    char buffer[200];
    rtc::SimpleStringBuilder name(buffer);
    if (IsAudioSsrc<Direction>(ssrc)) {
      name << "Audio ";
    } else if (IsVideoSsrc<Direction>(ssrc)) {
      name << "Video ";
    } else {
      name << "Unknown ";
    }
    if (IsRtxSsrc<Direction>(ssrc)) {
      name << "RTX ";
    }
    name << "(" << Direction::name << ") SSRC " << ssrc;
    return name.str();
  }

  template <typename Direction>
  rtc::Optional<uint32_t> EstimateRtpClockFrequency(
      const std::vector<typename Direction::RtpPacketType>& packets) const;

  float ToCallTime(int64_t timestamp) const;

  void Notification(std::unique_ptr<TriageNotification> notification);

  std::string GetCandidatePairLogDescriptionFromId(uint32_t candidate_pair_id);

  const ParsedRtcEventLog& parsed_log_;

  // A list of SSRCs we are interested in analysing.
  // If left empty, all SSRCs will be considered relevant.
  std::vector<uint32_t> desired_ssrc_;

  // Stores the timestamps for all log segments, in the form of associated start
  // and end events.
  std::vector<std::pair<int64_t, int64_t>> log_segments_;

  std::vector<std::unique_ptr<TriageNotification>> notifications_;

  std::map<uint32_t, std::string> candidate_pair_desc_by_id_;

  // Window and step size used for calculating moving averages, e.g. bitrate.
  // The generated data points will be |step_| microseconds apart.
  // Only events occuring at most |window_duration_| microseconds before the
  // current data point will be part of the average.
  int64_t window_duration_;
  int64_t step_;

  // First and last events of the log.
  int64_t begin_time_;
  int64_t end_time_;

  // Duration (in seconds) of log file.
  float call_duration_s_;
};

template <>
inline bool EventLogAnalyzer::IsRtxSsrc<Incoming>(uint32_t ssrc) const {
  return parsed_log_.incoming_rtx_ssrcs().find(ssrc) !=
         parsed_log_.incoming_rtx_ssrcs().end();
}

template <>
inline bool EventLogAnalyzer::IsVideoSsrc<Incoming>(uint32_t ssrc) const {
  return parsed_log_.incoming_video_ssrcs().find(ssrc) !=
         parsed_log_.incoming_video_ssrcs().end();
}

template <>
inline bool EventLogAnalyzer::IsAudioSsrc<Incoming>(uint32_t ssrc) const {
  return parsed_log_.incoming_audio_ssrcs().find(ssrc) !=
         parsed_log_.incoming_audio_ssrcs().end();
}

template <>
inline bool EventLogAnalyzer::IsRtxSsrc<Outgoing>(uint32_t ssrc) const {
  return parsed_log_.outgoing_rtx_ssrcs().find(ssrc) !=
         parsed_log_.outgoing_rtx_ssrcs().end();
}

template <>
inline bool EventLogAnalyzer::IsVideoSsrc<Outgoing>(uint32_t ssrc) const {
  return parsed_log_.outgoing_video_ssrcs().find(ssrc) !=
         parsed_log_.outgoing_video_ssrcs().end();
}

template <>
inline bool EventLogAnalyzer::IsAudioSsrc<Outgoing>(uint32_t ssrc) const {
  return parsed_log_.outgoing_audio_ssrcs().find(ssrc) !=
         parsed_log_.outgoing_audio_ssrcs().end();
}

}  // namespace rtceventlog
}  // namespace webrtc

#endif  // RTC_TOOLS_EVENT_LOG_VISUALIZER_ANALYZER_H_
