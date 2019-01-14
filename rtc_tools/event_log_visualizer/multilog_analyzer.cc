/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/event_log_visualizer/multilog_analyzer.h"

namespace webrtc {

namespace {

int64_t CalculateClockOffset(const ParsedRtcEventLog& log1,
                             const ParsedRtcEventLog& log2) {
  // TODO(zstein): Real implementation;
  return log2.first_timestamp() - log1.first_timestamp();
}

}  // namespace

MultiEventLogAnalyzer::MultiEventLogAnalyzer(const ParsedRtcEventLog& log1,
                                             const ParsedRtcEventLog& log2)
    : log1_(log1),
      log2_(log2),
      clock_offset_(CalculateClockOffset(log1, log2)) {
  std::cout << "# Clock offset: " << clock_offset_ << std::endl;
}

void MultiEventLogAnalyzer::CreateIceSequenceDiagrams(
    PlotCollection* plot_collection) {
  // TODO(zstein): Implement.

  // Prevents "not used" compiler errors.
  const void* x = reinterpret_cast<const void*>(&log1_);
  const void* y = reinterpret_cast<const void*>(&log2_);
  x = y;
  y = x;
}

void MultiEventLogAnalyzer::CreateIceTransactionGraphs(
    PlotCollection* plot_collection) {
  // TODO(zstein): Implement.
}

void MultiEventLogAnalyzer::CreateIceTransactionStateGraphs(
    PlotCollection* plot_collection) {
  // TODO(zstein): Implement.
}

void MultiEventLogAnalyzer::CreateIceTransactionRttGraphs(
    PlotCollection* plot_collection) {
  // TODO(zstein): Implement.
}

}  // namespace webrtc
