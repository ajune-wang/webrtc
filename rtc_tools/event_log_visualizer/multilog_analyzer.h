/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_EVENT_LOG_VISUALIZER_MULTILOG_ANALYZER_H_
#define RTC_TOOLS_EVENT_LOG_VISUALIZER_MULTILOG_ANALYZER_H_

#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_tools/event_log_visualizer/plot_base.h"

namespace webrtc {

class MultiEventLogAnalyzer {
 public:
  MultiEventLogAnalyzer(const ParsedRtcEventLog& log1,
                        const ParsedRtcEventLog& log2);

  // Y-axis is client id. Draws a point for each event, connected by transaction
  // id.
  void CreateIceSequenceDiagrams(PlotCollection* plot_collection);

  // Y-axis is IceCandidatePairEventType. Draws a point for each event,
  // connected by transaction id.
  void CreateIceTransactionGraphs(PlotCollection* plot_collection);

  // Y-axis is transaction state reached (max(IceCandidatePairEventType)). Draws
  // a point for each transaction id.
  void CreateIceTransactionStateGraphs(PlotCollection* plot_collection);

  // Y-axis is transaction RTT. X-axis is time transaction started. Draws a
  // point for each completed transaction.
  // TODO(zstein): Incorporate incomplete transactions.
  void CreateIceTransactionRttGraphs(PlotCollection* plot_collection);

 private:
  const ParsedRtcEventLog& log1_;
  const ParsedRtcEventLog& log2_;

  const int64_t clock_offset_;
};

}  // namespace webrtc

#endif  // RTC_TOOLS_EVENT_LOG_VISUALIZER_MULTILOG_ANALYZER_H_
