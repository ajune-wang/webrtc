/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/metrics/metrics_set_proto_file_exporter.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "api/test/metrics/metric.h"
#include "api/test/metrics/proto/metric.pb.h"
#include "api/units/timestamp.h"
#include "rtc_base/protobuf_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {

using ::testing::Eq;
using ::testing::Test;

std::string ReadFileAsString(const std::string& filename) {
  std::ifstream stream(filename.c_str());
  std::stringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

std::map<std::string, std::string> DefaultMetadata() {
  return std::map<std::string, std::string>{{"key", "value"}};
}

Metric::TimeSeries::Sample Sample(double value) {
  return Metric::TimeSeries::Sample{.timestamp = Timestamp::Seconds(1),
                                    .value = value,
                                    .sample_metadata = DefaultMetadata()};
}

class MetricsSetProtoFileExporterTest : public Test {
 protected:
  ~MetricsSetProtoFileExporterTest() override = default;

  void SetUp() override {
    temp_filename_ = webrtc::test::TempFilename(
        webrtc::test::OutputPath(), "metrics_set_proto_file_exporter_test");
  }

  void TearDown() override { remove(temp_filename_.c_str()); }

  std::string temp_filename_;
};

TEST_F(MetricsSetProtoFileExporterTest, DoThings) {
  MetricsSetProtoFileExporter::Options options(temp_filename_);
  MetricsSetProtoFileExporter exporter(options);

  Metric metric1{
      .name = "test_metric1",
      .unit = Unit::kTimeMs,
      .improvement_direction = ImprovementDirection::kBiggerIsBetter,
      .test_case = "test_case_name1",
      .metric_metadata = DefaultMetadata(),
      .time_series =
          Metric::TimeSeries{.samples = std::vector{Sample(10), Sample(20)}},
      .stats =
          Metric::Stats{.mean = 15.0, .stddev = 5.0, .min = 10.0, .max = 20.0}};
  Metric metric2{
      .name = "test_metric2",
      .unit = Unit::kKilobitsPerSecond,
      .improvement_direction = ImprovementDirection::kSmallerIsBetter,
      .test_case = "test_case_name2",
      .metric_metadata = DefaultMetadata(),
      .time_series =
          Metric::TimeSeries{.samples = std::vector{Sample(20), Sample(40)}},
      .stats = Metric::Stats{
          .mean = 30.0, .stddev = 10.0, .min = 20.0, .max = 40.0}};

  ASSERT_TRUE(exporter.Export(std::vector<Metric>{metric1, metric2}));
  webrtc::test_metrics::MetricsSet actual_metrics_set;
  actual_metrics_set.ParseFromString(ReadFileAsString(temp_filename_));
  EXPECT_THAT(actual_metrics_set.metrics().size(), Eq(2));
}

}  // namespace
}  // namespace test
}  // namespace webrtc
