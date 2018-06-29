/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "sdk/android/src/jni/logging/libjingle_logsink.h"

#include <android/log.h>
// Android has a 1024 limit on log inputs. We use 60 chars as an
// approx for the header/tag portion.
// See android/system/core/liblog/logd_write.c
static const int kMaxLogLineSize = 1024 - 60;

namespace webrtc {

LibjingleLogSink::LibjingleLogSink() = default;
LibjingleLogSink::~LibjingleLogSink() = default;

void LibjingleLogSink::OnLogMessage(const std::string& msg,
                                    rtc::LoggingSeverity severity,
                                    const char* old_tag) {
  bool log_to_stderr = log_to_stderr_;
  // Android's logging facility uses severity to log messages but we
  // need to map libjingle's severity levels to Android ones first.
  // Also write to stderr which maybe available to executable started
  // from the shell.
  int prio;
  switch (severity) {
    case LS_SENSITIVE:
      __android_log_write(ANDROID_LOG_INFO, override_tag, "SENSITIVE");
      return;
    case LS_VERBOSE:
      prio = ANDROID_LOG_VERBOSE;
      break;
    case LS_INFO:
      prio = ANDROID_LOG_INFO;
      break;
    case LS_WARNING:
      prio = ANDROID_LOG_WARN;
      break;
    case LS_ERROR:
      prio = ANDROID_LOG_ERROR;
      break;
    default:
      prio = ANDROID_LOG_UNKNOWN;
  }

  int size = str.size();
  int line = 0;
  int idx = 0;
  const int max_lines = size / kMaxLogLineSize + 1;
  if (max_lines == 1) {
    __android_log_print(prio, override_tag_, "%s: %.*s", old_tag, size,
                        str.c_str());
  } else {
    while (size > 0) {
      const int len = std::min(size, kMaxLogLineSize);
      // Use the size of the string in the format (str may have \0 in the
      // middle).
      __android_log_print(prio, override_tag_, "%s: [%d/%d] %.*s", old_tag,
                          line + 1, max_lines, len, str.c_str() + idx);
      idx += len;
      size -= len;
      ++line;
    }
  }
  if (log_to_stderr) {
    fprintf(stderr, "%s: %s", old_tag, str.c_str());
    fflush(stderr);
  }
}

void LibjingleLogSink::OnLogMessage(const std::string& msg) {
  RTC_NOTREACHED();
}

}  // namespace webrtc
