/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/scoped_temp_dir.h"

#include <string>

#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {

ScopedTempDir::~ScopedTempDir() {
  RemoveDir(temp_dir_path_);
}

absl::optional<std::string> ScopedTempDir::Create(absl::string_view name) {
  rtc::StringBuilder path;
  path << GetOsTempDir() << name << kPathDelimiter;
  temp_dir_path_ = path.Release();

  // Explicitly checking the directory doesn't exist because CreateDir
  // doesn't complain in that case.
  // TODO(mbonadei): Fix CreateDir.
  if (DirExists(temp_dir_path_)) {
    RTC_LOG(INFO) << "Path already exists: " << temp_dir_path_;
    return absl::nullopt;
  }

  if (CreateDir(temp_dir_path_)) {
    return temp_dir_path_;
  }
  return absl::nullopt;
}

}  // namespace test
}  // namespace webrtc
