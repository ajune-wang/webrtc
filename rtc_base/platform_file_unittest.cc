/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/platform_file.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace rtc {

TEST(PlatformFileTest, CreateWriteAndDelete) {
  std::string filename = webrtc::test::GenerateTempFilename(
      webrtc::test::OutputPath(), ".testfile");
  PlatformFile fd = rtc::CreatePlatformFile(filename);
  ASSERT_NE(fd, rtc::kInvalidPlatformFileValue)
      << "Failed to create file descriptor for file: " << filename;
  FILE* file = rtc::FdopenPlatformFile(fd, "w");
  ASSERT_TRUE(file != NULL) << "Failed to open file: " << filename;
  ASSERT_GT(fprintf(file, "%s", "Dummy data"), 0)
      << "Failed to write to file: " << filename;
  fclose(file);
}

TEST(PlatformFileTest, OpenExistingWriteAndDelete) {
  std::string filename = webrtc::test::GenerateTempFilename(
      webrtc::test::OutputPath(), ".testfile");

  // Create file with dummy data.
  FILE* file = fopen(filename.c_str(), "wb");
  ASSERT_TRUE(file != NULL) << "Failed to open file: " << filename;
  ASSERT_GT(fprintf(file, "%s", "Dummy data"), 0)
      << "Failed to write to file: " << filename;
  fclose(file);

  // Open it for write, write and delete.
  PlatformFile fd = rtc::OpenPlatformFile(filename);
  ASSERT_NE(fd, rtc::kInvalidPlatformFileValue)
      << "Failed to open file descriptor for file: " << filename;
  file = rtc::FdopenPlatformFile(fd, "w");
  ASSERT_TRUE(file != NULL) << "Failed to open file: " << filename;
  ASSERT_GT(fprintf(file, "%s", "Dummy data"), 0)
      << "Failed to write to file: " << filename;
  fclose(file);
}

TEST(PlatformFileTest, OpenExistingReadOnlyAndDelete) {
  std::string filename = webrtc::test::GenerateTempFilename(
      webrtc::test::OutputPath(), ".testfile");

  // Create file with dummy data.
  FILE* file = fopen(filename.c_str(), "wb");
  ASSERT_TRUE(file != NULL) << "Failed to open file: " << filename;
  ASSERT_GT(fprintf(file, "%s", "Dummy data"), 0)
      << "Failed to write to file: " << filename;
  fclose(file);

  // Open it for write, write and delete.
  PlatformFile fd = rtc::OpenPlatformFileReadOnly(filename);
  ASSERT_NE(fd, rtc::kInvalidPlatformFileValue)
      << "Failed to open file descriptor for file: " << filename;
  file = rtc::FdopenPlatformFile(fd, "r");
  ASSERT_TRUE(file != NULL) << "Failed to open file: " << filename;

  int buf[]{0};
  ASSERT_GT(fread(&buf, 1, 1, file), static_cast<size_t>(0))
      << "Failed to read from file: " << filename;
  fclose(file);
}

}  // namespace rtc
