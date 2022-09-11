/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_METRICS_STDOUT_METRICS_EXPORTER_H_
#define TEST_TESTSUPPORT_METRICS_STDOUT_METRICS_EXPORTER_H_

#include "api/array_view.h"
#include "test/testsupport/metrics/metric.h"
#include "test/testsupport/metrics/metrics_exporter.h"

namespace webrtc {
namespace test {

// Exports all collected metrics into stdout.
class StdoutMetricsExporter : public MetricsExporter {
 public:
  StdoutMetricsExporter();
  ~StdoutMetricsExporter() override = default;

  bool Export(rtc::ArrayView<const Metric> metrics) override;

 private:
  void PrintMetric(const Metric& metric);

  FILE* output_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_METRICS_STDOUT_METRICS_EXPORTER_H_
