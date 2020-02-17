/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "test/gtest.h"
#include "test/ios/test_support.h"
#include "test/test_main_lib.h"

int main(int argc, char* argv[]) {
  // Initialize the symbolizer to get a human-readable stack trace
  // TODO(crbug.com/1050976): Breaks iossim tests, re-enable when fixed.
  // absl::InitializeSymbolizer(argv[0]);

  // absl::FailureSignalHandlerOptions options;
  // absl::InstallFailureSignalHandler(options);

  rtc::test::InitTestSuite(RUN_ALL_TESTS, argc, argv, false);
  rtc::test::RunTestsFromIOSApp();
  return 0;
}
