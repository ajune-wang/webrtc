/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/numerics/time_series_set_exporter.h"

#include <memory>

#include "api/numerics/time_series_set_exporter_create.h"
#if WEBRTC_ENABLE_PROTOBUF
#include "rtc_base/numerics/time_series.pb.h"
#endif
#include "rtc_base/system/file_wrapper.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::Property;

#if WEBRTC_ENABLE_PROTOBUF

std::string Export(TimeSeriesSetExporter* exporter) {
  std::string output_path =
      test::GenerateTempFilename(test::OutputPath(), "tss-");
  EXPECT_TRUE(exporter->ExportToBinaryProtobuf(output_path));
  EXPECT_TRUE(test::FileExists(output_path));
  EXPECT_GT(test::GetFileSize(output_path), 0u);
  return output_path;
}

proto::TimeSeriesSet Read(const std::string& output_path) {
  FileWrapper file = FileWrapper::OpenReadOnly(output_path);
  EXPECT_TRUE(file.is_open());
  char buffer[1000];
  size_t len = file.Read(buffer, 1000);
  EXPECT_TRUE(file.ReadEof());  // Increase the buffer size if this fails.
  proto::TimeSeriesSet time_series_set;
  EXPECT_TRUE(time_series_set.ParseFromString(std::string(buffer, len)));
  return time_series_set;
}

TEST(TimeSeriesSetExporterTest, AddSampleSingleTimeSeries) {
  // Add some data.
  std::unique_ptr<TimeSeriesSetExporter> exporter =
      CreateTimeSeriesSetExporter("set1");
  exporter->AddSample("series1", Timestamp::Micros(1), 10.0);
  exporter->AddSample("series1", Timestamp::Micros(2), 20.0);

  // Export and read back.
  proto::TimeSeriesSet time_series_set = Read(Export(exporter.get()));

  // Verify.
  EXPECT_EQ(time_series_set.name(), "set1");
  ASSERT_EQ(time_series_set.timeseries_size(), 1);
  const proto::TimeSeries& time_series = time_series_set.timeseries(0);
  EXPECT_EQ(time_series.name(), "series1");
  EXPECT_THAT(time_series.timestamps_us(), ElementsAre(1, 2));
  EXPECT_THAT(time_series.values(), ElementsAre(10.0, 20.0));
  EXPECT_THAT(time_series.annotations(), ElementsAre("", ""));
}

TEST(TimeSeriesSetExporterTest, AddSampleMultipleTimeSeries) {
  // Add some data.
  std::unique_ptr<TimeSeriesSetExporter> exporter =
      CreateTimeSeriesSetExporter("set1");
  exporter->AddSample("series1", Timestamp::Micros(1), 10.0);
  exporter->AddSample("series1", Timestamp::Micros(2), 20.0);
  exporter->AddSample("series2", Timestamp::Micros(12), 34.5);
  exporter->AddSample("series2", Timestamp::Micros(78), 89.0);

  // Export and read back.
  proto::TimeSeriesSet time_series_set = Read(Export(exporter.get()));

  // Verify.
  EXPECT_EQ(time_series_set.name(), "set1");
  ASSERT_EQ(time_series_set.timeseries_size(), 2);
  const proto::TimeSeries& time_series1 = time_series_set.timeseries(0);
  EXPECT_EQ(time_series1.name(), "series1");
  EXPECT_THAT(time_series1.timestamps_us(), ElementsAre(1, 2));
  EXPECT_THAT(time_series1.values(), ElementsAre(10.0, 20.0));
  EXPECT_THAT(time_series1.annotations(), ElementsAre("", ""));
  const proto::TimeSeries& time_series2 = time_series_set.timeseries(1);
  EXPECT_EQ(time_series2.name(), "series2");
  EXPECT_THAT(time_series2.timestamps_us(), ElementsAre(12, 78));
  EXPECT_THAT(time_series2.values(), ElementsAre(34.5, 89.0));
  EXPECT_THAT(time_series2.annotations(), ElementsAre("", ""));
}

TEST(TimeSeriesSetExporterTest, AddAnnotatedSample) {
  // Add some data.
  std::unique_ptr<TimeSeriesSetExporter> exporter =
      CreateTimeSeriesSetExporter("set1");
  exporter->AddSample("series1", Timestamp::Micros(1), 10.0, "a");
  exporter->AddSample("series1", Timestamp::Micros(2), 20.0, "b");

  // Export and read back.
  proto::TimeSeriesSet time_series_set = Read(Export(exporter.get()));

  // Verify.
  ASSERT_EQ(time_series_set.timeseries_size(), 1);
  const proto::TimeSeries& time_series = time_series_set.timeseries(0);
  EXPECT_THAT(time_series.annotations(), ElementsAre("a", "b"));
}

TEST(TimeSeriesSetExporterTest, InsertOrderIsMaintained) {
  // Add some data.
  std::unique_ptr<TimeSeriesSetExporter> exporter =
      CreateTimeSeriesSetExporter("set1");
  exporter->AddSample("first", Timestamp::Micros(1), 10.0);
  exporter->AddSample("second", Timestamp::Micros(2), 20.0);
  exporter->AddSample("third", Timestamp::Micros(3), 30.0);
  exporter->AddSample("second", Timestamp::Micros(4), 40.0);
  exporter->AddSample("first", Timestamp::Micros(5), 50.0);

  // Export and read back.
  proto::TimeSeriesSet time_series_set = Read(Export(exporter.get()));

  // Verify.
  EXPECT_THAT(time_series_set.timeseries(),
              ElementsAre(Property(&proto::TimeSeries::name, "first"),
                          Property(&proto::TimeSeries::name, "second"),
                          Property(&proto::TimeSeries::name, "third")));
}

#endif  // WEBRTC_ENABLE_PROTOBUF

}  // namespace
}  // namespace webrtc
