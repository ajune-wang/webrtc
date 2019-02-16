/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/scoped_temp_dir.h"

namespace webrtc {
namespace test {
namespace {

TEST(ScopedTempDirTest, TestEmptyDir) {
  { ScopedTempDir temp_dir; }
}

TEST(ScopedTempDirTest, TestCreationAndDestruction) {
  std::string temp_dir_path;
  {
    ScopedTempDir temp_dir;
    absl::optional<std::string> temp_dir_path_opt = temp_dir.Create("foo");
    ASSERT_NE(temp_dir_path_opt, absl::nullopt);
    temp_dir_path = temp_dir_path_opt.value();
    ASSERT_TRUE(DirExists(temp_dir_path));
  }
  ASSERT_FALSE(DirExists(temp_dir_path));
}

TEST(ScopedTempDirTest, TestCreateAlreadyExisting) {
  std::string temp_dir_path;
  {
    ScopedTempDir temp_dir;
    absl::optional<std::string> temp_dir_path_opt = temp_dir.Create("foo");
    {
      ScopedTempDir dup_temp_dir;
      absl::optional<std::string> dup_dir_path_opt = dup_temp_dir.Create("foo");
      ASSERT_EQ(dup_dir_path_opt, absl::nullopt);
    }
  }
}

TEST(ScopedTempDirTest, TestCreateWithEmptyString) {
  {
    ScopedTempDir temp_dir;
    ASSERT_EQ(temp_dir.Create(""), absl::nullopt);
  }
}

TEST(ScopedTempDirTest, TestNestedScopedTempDirs) {
  std::string root_temp_dir_path;
  std::string child_temp_dir1_path;
  std::string child_temp_dir2_path;
  {
    ScopedTempDir root_temp_dir;
    ScopedTempDir child_temp_dir1;
    ScopedTempDir child_temp_dir2;

    absl::optional<std::string> temp_dir_path_opt = root_temp_dir.Create("foo");
    ASSERT_NE(temp_dir_path_opt, absl::nullopt);
    root_temp_dir_path = temp_dir_path_opt.value();

    // TODO(mbonadei): Usa OS neutral path separators.
    absl::optional<std::string> child_temp_dir1_path_opt =
        child_temp_dir1.Create("foo/bar");
    ASSERT_NE(child_temp_dir1_path_opt, absl::nullopt);
    absl::optional<std::string> child_temp_dir2_path_opt =
        child_temp_dir2.Create("foo/baz");
    ASSERT_NE(child_temp_dir2_path_opt, absl::nullopt);
  }
  ASSERT_FALSE(DirExists(root_temp_dir_path));
  ASSERT_FALSE(DirExists(child_temp_dir1_path));
  ASSERT_FALSE(DirExists(child_temp_dir2_path));
}

}  // namespace
}  // namespace test
}  // namespace webrtc
