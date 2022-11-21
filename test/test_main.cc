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
#include <regex>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/parse.h"
#include "test/gmock.h"
#include "test/test_main_lib.h"

std::vector<std::string> ReplaceDashesByUnderscores(int argc, char* argv[]) {
  std::vector<std::string> args(argv, argv + argc);
  for (int i = 1; i < argc; ++i) {
    fprintf(stderr, "1 - %s\n", args[i].c_str());
    args[i] = std::regex_replace(args[i], std::regex("[^-]-"), "$1_");
    fprintf(stderr, "2 - %s\n", args[i].c_str());
  }
  return args;
}

std::vector<char*> VectorOfStringsToPointers(std::vector<std::string>& input) {
  std::vector<char*> output(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    output[i] = &(input[i][0]);
    fprintf(stderr, "--> %s\n", input[i].c_str());
  }
  return output;
}

int main(int argc, char* argv[]) {
  // Initialize the symbolizer to get a human-readable stack trace
  absl::InitializeSymbolizer(argv[0]);
  testing::InitGoogleMock(&argc, argv);
  std::vector<std::string> new_argv = ReplaceDashesByUnderscores(argc, argv);
  absl::ParseCommandLine(argc, &VectorOfStringsToPointers(new_argv)[0]);

// This absl handler use unsupported features/instructions on Fuchsia
#if !defined(WEBRTC_FUCHSIA)
  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);
#endif

  std::unique_ptr<webrtc::TestMain> main = webrtc::TestMain::Create();
  int err_code = main->Init();
  if (err_code != 0) {
    return err_code;
  }
  return main->Run(argc, argv);
}
