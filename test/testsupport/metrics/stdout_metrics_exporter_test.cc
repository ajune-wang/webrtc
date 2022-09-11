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

#include <map>
#include <string>
#include <vector>

#include "api/units/timestamp.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/metrics/metric.h"

namespace webrtc {
namespace test {
namespace {

std::map<std::string, std::string> DefaultMetadata() {
  return std::map<std::string, std::string>{{"key", "value"}};
}

Metric::TimeSeries::DataPoint DataPoint(double value) {
  return Metric::TimeSeries::DataPoint{.timestamp = Timestamp::Seconds(1),
                                       .value = value,
                                       .metadata = DefaultMetadata()};
}

#if defined(WEBRTC_IOS)
#define MAYBE_ExportMetricFormatCorrect DISABLED_ExportMetricFormatCorrect
#else
#define MAYBE_ExportMetricFormatCorrect ExportMetricFormatCorrect
#endif
TEST(StdoutMetricsExporterTest, MAYBE_ExportMetricFormatCorrect) {
  Metric metric1{
      .name = "test_metric1",
      .unit = Unit::kTimeMs,
      .improvement_direction = ImprovementDirection::kBiggerIsBetter,
      .test_case = "test_case_name1",
      .metadata = DefaultMetadata(),
      .time_series = Metric::TimeSeries{.values = std::vector{DataPoint(10),
                                                              DataPoint(20)}},
      .stats =
          Metric::Stats{.mean = 15.0, .stddev = 5.0, .min = 10.0, .max = 20.0}};
  Metric metric2{
      .name = "test_metric2",
      .unit = Unit::kKilobitsPerSecond,
      .improvement_direction = ImprovementDirection::kSmallerIsBetter,
      .test_case = "test_case_name2",
      .metadata = DefaultMetadata(),
      .time_series = Metric::TimeSeries{.values = std::vector{DataPoint(20),
                                                              DataPoint(40)}},
      .stats = Metric::Stats{
          .mean = 30.0, .stddev = 10.0, .min = 20.0, .max = 40.0}};

  testing::internal::CaptureStdout();
  StdoutMetricsExporter exporter;

  std::string expected =
      "RESULT: test_case_name1/test_metric1= "
      "{mean=15, stddev=5} TimeMs (BiggerIsBetter)\n"
      "RESULT: test_case_name2/test_metric2= "
      "{mean=30, stddev=10} KilobitsPerSecond (SmallerIsBetter)\n";

  EXPECT_TRUE(exporter.Export(std::vector<Metric>{metric1, metric2}));
  EXPECT_EQ(expected, testing::internal::GetCapturedStdout());
}

}  // namespace
}  // namespace test
}  // namespace webrtc
