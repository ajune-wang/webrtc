/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef LIBJINGLE_LOGSINK_H_
#define LIBJINGLE_LOGSINK_H_

#include <string>

#include "rtc_base/logging.h"

namespace webrtc {

class LibjingleLogSink : public rtc::LogSink {
 public:
  LibjingleLogSink();
  ~LibjingleLogSink() override;

  void OnLogMessage(const std::string& msg,
                    rtc::LoggingSeverity severity,
                    const char* tag) override;
  void OnLogMessage(const std::string& msg) override;

 private:
  const char* override_tag = "libjingle";
  bool log_to_stderr_ = true;
};

}  // namespace webrtc
#endif  // LIBJINGLE_LOGSINK_H
