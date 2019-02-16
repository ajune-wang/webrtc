/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_SCOPED_TEMP_DIR_H_
#define TEST_TESTSUPPORT_SCOPED_TEMP_DIR_H_

#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "rtc_base/system/unused.h"

namespace webrtc {
namespace test {

// RAII wrapper around a directory in the OS temporary storage directory
// (e.g. "/tmp" on UNIX).
// The directory is not created automatically at construction time and the
// user needs to call ScopedTempDir::Create in order to actually create
// the directory on the file system.
// When this object goes out of scope, the directory is removed from the
// file system. Since deletion happens in the destructor, no error handling
// is done by this class in case the directory fails to be deleted. On the
// other hand, the directory is created in a storage space that should be
// automatically cleaned up on reboot, or at other appropriate times).
class ScopedTempDir {
 public:
  ~ScopedTempDir();

  absl::optional<std::string> Create(const absl::string_view name)
      RTC_WARN_UNUSED_RESULT;

 private:
  std::string temp_dir_path_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_SCOPED_TEMP_DIR_H_
