/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <string>

#include "absl/strings/string_view.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/flags.h"

namespace {
WEBRTC_DEFINE_bool(help, false, "Prints this message");

void PrintSsrcs(absl::string_view description,
                const std::set<uint32_t>& ssrcs) {
  std::cout << description;
  std::string separator;
  for (const auto ssrc : ssrcs) {
    std::cout << separator << ssrc;
    separator = ", ";
  }
  std::cout << std::endl;
}
}  // namespace

int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Tool that prints which SSRCs are in an event log.\n"
      "Example usage:\n" +
      program_name + " event_log.log\n";
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true)) {
    exit(1);
  }
  if (FLAG_help) {
    std::cout << usage;
    rtc::FlagList::Print(nullptr, false);
    exit(0);
  }
  if (argc != 2) {
    // Print usage information.
    std::cerr << usage;
    exit(1);
  }
  const std::string input_file_name(argv[1]);
  webrtc::ParsedRtcEventLog parsed_log;
  RTC_CHECK(parsed_log.ParseFile(input_file_name));
  PrintSsrcs("Incoming audio: ", parsed_log.incoming_audio_ssrcs());
  PrintSsrcs("Incoming video: ", parsed_log.incoming_video_ssrcs());
  PrintSsrcs("Incoming rtx: ", parsed_log.incoming_rtx_ssrcs());
  PrintSsrcs("Outgoing audio: ", parsed_log.outgoing_audio_ssrcs());
  PrintSsrcs("Outgoing video: ", parsed_log.outgoing_video_ssrcs());
  PrintSsrcs("Outgoing rtx: ", parsed_log.outgoing_rtx_ssrcs());
  std::cout << std::endl;
  return 0;
}
