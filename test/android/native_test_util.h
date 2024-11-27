/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_ANDROID_NATIVE_TEST_UTIL_H_
#define TEST_ANDROID_NATIVE_TEST_UTIL_H_

#include <stdio.h>

#include <string>
#include <vector>

// Helper methods for setting up environment for running gtest tests
// inside an APK.
namespace testing {
namespace android {

class ScopedMainEntryLogger {
 public:
  ScopedMainEntryLogger() { printf(">>ScopedMainEntryLogger\n"); }

  ~ScopedMainEntryLogger() {
    printf("<<ScopedMainEntryLogger\n");
    fflush(stdout);
    fflush(stderr);
  }
};

void ParseArgsFromString(const std::string& command_line,
                         std::vector<std::string>* args);
void ParseArgsFromCommandLineFile(const char* path,
                                  std::vector<std::string>* args);
int ArgsToArgv(const std::vector<std::string>& args, std::vector<char*>* argv);

}  // namespace android
}  // namespace testing

#endif  // TEST_ANDROID_NATIVE_TEST_UTIL_H_
