/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <sys/socket.h>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "net/dcsctp/compat/dcsctp_compat_server.h"
#include "rtc_base/logging.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/thread.h"

ABSL_FLAG(bool, verbose, false, "verbose logs to stderr");
ABSL_FLAG(std::string, command_addr, "127.0.0.1:5675", "Command listen addr.");

int main(int argc, char* argv[]) {
  absl::InitializeSymbolizer(argv[0]);
  absl::ParseCommandLine(argc, argv);

  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  if (rtc::LogMessage::GetLogToDebug() > rtc::LS_INFO)
    rtc::LogMessage::LogToDebug(rtc::LS_INFO);

  if (absl::GetFlag(FLAGS_verbose))
    rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);

  rtc::SocketAddress command_address;
  RTC_CHECK(command_address.FromString(absl::GetFlag(FLAGS_command_addr)))
      << "Failed to parse command address";

  std::unique_ptr<rtc::PhysicalSocketServer> pss(
      new rtc::PhysicalSocketServer());
  rtc::Thread thread(pss.get());
  dcsctp::compat::DcsctpCompatServer server(&thread, command_address);

  RTC_LOG(LS_WARNING) << "Awaiting connections on "
                      << command_address.ToString();

  pss->Wait(rtc::SocketServer::kForever, true);

  RTC_LOG(LS_WARNING) << "Exiting...";
}
