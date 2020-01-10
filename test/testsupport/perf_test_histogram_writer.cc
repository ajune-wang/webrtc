/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_test_histogram_writer.h"

#include <stdlib.h>

#include <map>

namespace webrtc {
namespace test {

namespace {

/*class PerfTestHistogramWriter : public PerfTestResultWriter {
 public:
  PerfTestHistogramWriter() : crit_(), output_(stdout) {}
  void ClearResults() override {
    rtc::CritScope lock(&crit_);
    histograms_.clear();
  }
  void LogResult(const std::string& graph_name,
                 const std::string& trace_name,
                 const double value,
                 const std::string& units,
                 const bool important,
                 webrtc::test::ImproveDirection improve_direction) override {
    if (histograms_.count(graph_name) == 0) {
      histograms_[graph_name] =
          catapult::HistogramBuilder(graph_name, _ParseUnit(units));
    }
    histograms_[graph_name].AddSample(value);
    (void)important;  // Ignored on purpose.
  }
  void LogResultMeanAndError(
      const std::string& graph_name,
      const std::string& trace_name,
      const double mean,
      const double error,
      const std::string& units,
      const bool important,
      webrtc::test::ImproveDirection improve_direction) override {
    RTC_NOTREACHED() << "Not implemented, this should go away";
  }
  void LogResultList(
      const std::string& graph_name,
      const std::string& trace_name,
      const rtc::ArrayView<const double> values,
      const std::string& units,
      const bool important,
      webrtc::test::ImproveDirection improve_direction) override {
    RTC_NOTREACHED() << "Not implemented, this should go away";
  }
  std::string ToJSON() const override {
  }

  std::map<std::string, catapult::HistogramBuilder> histograms_;
};*/

}  // namespace

PerfTestResultWriter* CreateHistogramWriter() {
  return nullptr;
}

}  // namespace test
}  // namespace webrtc
