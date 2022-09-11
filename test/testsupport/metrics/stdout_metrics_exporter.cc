/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/testsupport/metrics/stdout_metrics_exporter.h"

#include <stdio.h>

#include <sstream>
#include <string>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "test/testsupport/metrics/metric.h"

namespace webrtc {
namespace test {

StdoutMetricsExporter::StdoutMetricsExporter() : output_(stdout) {}

bool StdoutMetricsExporter::Export(rtc::ArrayView<const Metric> metrics) {
  for (const Metric& metric : metrics) {
    PrintMetric(metric);
  }
  return true;
}

void StdoutMetricsExporter::PrintMetric(const Metric& metric) {
  std::ostringstream value_stream;
  value_stream.precision(8);
  value_stream << metric.test_case << "/" << metric.name << "= {mean=";
  if (metric.stats.mean.has_value()) {
    value_stream << *metric.stats.mean;
  } else {
    value_stream << "-";
  }
  value_stream << ", stddev=";
  if (metric.stats.stddev.has_value()) {
    value_stream << *metric.stats.stddev;
  } else {
    value_stream << "-";
  }
  value_stream << "} " << ToString(metric.unit) << " ("
               << ToString(metric.improvement_direction) << ")";

  fprintf(output_, "RESULT: %s\n", value_stream.str().c_str());
}

}  // namespace test
}  // namespace webrtc
