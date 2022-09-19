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

#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {

using ::testing::Test;

class MetricsSetProtoFileExporterTest : public Test {
 protected:
  ~MetricsSetProtoFileExporterTest() override = default;

  void SetUp() override {
    temp_filename_ = webrtc::test::TempFilename(
        webrtc::test::OutputPath(),
        "metrics_set_proto_file_exporter_test");
  }

  void TearDown() override { remove(temp_filename_.c_str()); }

  std::string temp_filename_;
};

TEST_F(MetricsSetProtoFileExporterTest, DoThings) {
  MetricsSetProtoFileExporter::Options options(temp_filename_);
  MetricsSetProtoFileExporter exporter(options);
}

}  // namespace
}  // namespace test
}  // namespace webrtc