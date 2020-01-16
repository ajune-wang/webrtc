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

#include <memory>
#include <string>

#include "test/gtest.h"

namespace webrtc {
namespace test {

TEST(PerfHistogramWriterUnittest, WritesSimpleValue) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("measurement", "user_story", 15e7, "Hz", false,
                    ImproveDirection::kBiggerIsBetter);

  EXPECT_EQ(R"({
 "histograms": [
  {
   "name": "measurement",
   "unit": {
    "unit": "HERTZ",
    "improvementDirection": "BIGGER_IS_BETTER"
   },
   "diagnostics": {
    "diagnosticMap": {
     "stories": {
      "genericSet": {
       "values": [
        "\"user_story\""
       ]
      }
     }
    }
   },
   "sampleValues": [
    1.5e+08
   ],
   "maxNumSampleValues": 10,
   "running": {
    "count": 1,
    "max": 1.5e+08,
    "meanlogs": 18.8261452,
    "mean": 1.5e+08,
    "min": 1.5e+08,
    "sum": 1.5e+08
   }
  }
 ]
}
)",
            writer->ToJSON());
}

TEST(PerfHistogramWriterUnittest, IgnoresError) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResultMeanAndError("-", "-", 17, 12345, "-", false,
                                ImproveDirection::kNone);

  std::string json = writer->ToJSON();
  EXPECT_NE(json.find("17"), std::string::npos)
      << "Sample value should be somewhere in the file";
  EXPECT_EQ(json.find("12345"), std::string::npos)
      << "Error should be thrown away";
}

TEST(PerfHistogramWriterUnittest, WritesDecibelIntoMeasurementName) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("measurement", "-", 0, "dB", false,
                    ImproveDirection::kNone);

  std::string json = writer->ToJSON();
  EXPECT_NE(json.find(R"("unit": "UNITLESS")"), std::string::npos)
      << "dB should map to unitless" << json;
  EXPECT_NE(json.find(R"("name": "measurement_dB")"), std::string::npos)
      << "measurement should be renamed" << json;
}

TEST(PerfHistogramWriterUnittest, WritesFpsIntoMeasurementName) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("measurement", "-", 0, "fps", false,
                    ImproveDirection::kNone);

  std::string json = writer->ToJSON();
  EXPECT_NE(json.find(R"("unit": "HERTZ")"), std::string::npos)
      << "fps should map to hertz" << json;
  EXPECT_NE(json.find(R"("name": "measurement_fps")"), std::string::npos)
      << "measurement should be renamed" << json;
}

TEST(PerfHistogramWriterUnittest, WritesPercentIntoMeasurementName) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("measurement", "-", 0, "%", false, ImproveDirection::kNone);

  std::string json = writer->ToJSON();
  EXPECT_NE(json.find(R"("unit": "UNITLESS")"), std::string::npos)
      << "fps should map to unitless" << json;
  EXPECT_NE(json.find(R"("name": "measurement_%")"), std::string::npos)
      << "measurement should be renamed" << json;
}

TEST(PerfHistogramWriterUnittest, BitsPerSecondIsConvertedToBytes) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("-", "-", 1024, "bps", false, ImproveDirection::kNone);

  std::string json = writer->ToJSON();
  EXPECT_NE(json.find("128"), std::string::npos)
      << "1024 bits = 128 bytes" << json;
  EXPECT_EQ(json.find("1024"), std::string::npos) << json;
}

TEST(PerfHistogramWriterUnittest, ParsesDirection) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("measurement1", "-", 0, "bps", false,
                    ImproveDirection::kBiggerIsBetter);
  writer->LogResult("measurement2", "-", 0, "frames", false,
                    ImproveDirection::kSmallerIsBetter);
  writer->LogResult("measurement3", "-", 0, "sigma", false,
                    ImproveDirection::kNone);

  std::string json = writer->ToJSON();
  EXPECT_NE(json.find(R"("name": "measurement1",
   "unit": {
    "unit": "BYTES_PER_SECOND",
    "improvementDirection": "BIGGER_IS_BETTER")"),
            std::string::npos)
      << json;
  EXPECT_NE(json.find(R"("name": "measurement2",
   "unit": {
    "unit": "COUNT",
    "improvementDirection": "SMALLER_IS_BETTER")"),
            std::string::npos)
      << json;

  // In the case of kNone the improvement direction isn't set in the wire
  // format.
  EXPECT_NE(json.find(R"("name": "measurement3",
   "unit": {
    "unit": "SIGMA"
   })"),
            std::string::npos)
      << json;
}

}  // namespace test
}  // namespace webrtc
