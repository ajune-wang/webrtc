/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/logging.h"

#include <string.h>

#if RTC_LOG_ENABLED()

#if defined(WEBRTC_WIN)
#include <windows.h>
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#undef ERROR  // wingdi.h
#endif

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include <CoreServices/CoreServices.h>
#elif defined(WEBRTC_ANDROID)
#include <android/log.h>

// Android has a 1024 limit on log inputs. We use 60 chars as an
// approx for the header/tag portion.
// See android/system/core/liblog/logd_write.c
static const int kMaxLogLineSize = 1024 - 60;
#endif  // WEBRTC_MAC && !defined(WEBRTC_IOS) || WEBRTC_ANDROID

#include <inttypes.h>
#include <stdio.h>
#include <time.h>

#include <algorithm>
#include <cstdarg>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"

namespace rtc {
namespace {

// By default, release builds don't log, debug builds at info level
#if !defined(NDEBUG)
constexpr LoggingSeverity kDefaultLoggingSeverity = LS_INFO;
#else
constexpr LoggingSeverity kDefaultLoggingSeverity = LS_NONE;
#endif

// Note: `g_min_sev` and `g_dbg_sev` can be changed while running.
LoggingSeverity g_min_sev = kDefaultLoggingSeverity;
LoggingSeverity g_dbg_sev = kDefaultLoggingSeverity;

// Return the filename portion of the string (that following the last slash).
const char* FilenameFromPath(const char* file) {
  const char* end1 = ::strrchr(file, '/');
  const char* end2 = ::strrchr(file, '\\');
  if (!end1 && !end2)
    return file;
  else
    return (end1 > end2) ? end1 + 1 : end2 + 1;
}

// Global lock for log subsystem, only needed to serialize access to streams_.
webrtc::Mutex& GetLoggingLock() {
  static webrtc::Mutex& mutex = *new webrtc::Mutex();
  return mutex;
}

}  // namespace

std::string LogLineRef::DefaultLogLine() const {
  rtc::StringBuilder log_output;
  if (timestamp_ != webrtc::Timestamp::MinusInfinity()) {
    // TODO(kwiberg): Switch to absl::StrFormat, if binary size is ok.
    char timestamp[50];  // Maximum string length of an int64_t is 20.
    int len =
        snprintf(timestamp, sizeof(timestamp), "[%03" PRId64 ":%03" PRId64 "]",
                 timestamp_.ms() / 1000, timestamp_.ms() % 1000);
    RTC_DCHECK_LT(len, sizeof(timestamp));
    log_output << timestamp;
  }
  if (thread_id_.has_value()) {
    log_output << "[" << *thread_id_ << "] ";
  }
  if (!filename_.empty()) {
#if defined(WEBRTC_ANDROID)
    log_output << "(line " << line_ << "): ";
#else
    log_output << "(" << filename_ << ":" << line_ << "): ";
#endif
  }
  log_output << message_;
  return log_output.Release();
}

/////////////////////////////////////////////////////////////////////////////
// LogMessage
/////////////////////////////////////////////////////////////////////////////

bool LogMessage::log_to_stderr_ = true;

// The list of logging streams currently configured.
// Note: we explicitly do not clean this up, because of the uncertain ordering
// of destructors at program exit.  Let the person who sets the stream trigger
// cleanup by setting to null, or let it leak (safe at program exit).
ABSL_CONST_INIT LogSink* LogMessage::streams_ RTC_GUARDED_BY(GetLoggingLock()) =
    nullptr;
ABSL_CONST_INIT std::atomic<bool> LogMessage::streams_empty_ = {true};

// Boolean options default to false.
ABSL_CONST_INIT bool LogMessage::log_thread_ = false;
ABSL_CONST_INIT bool LogMessage::log_timestamp_ = false;

namespace webrtc_logging_impl {

LogStreamer::LogStreamer() {
  if (LogMessage::log_timestamp_) {
    int64_t log_start_time = LogMessage::LogStartTime();
    // Use SystemTimeMillis so that even if tests use fake clocks, the timestamp
    // in log messages represents the real system time.
    int64_t time = TimeDiff(SystemTimeMillis(), log_start_time);
    // Also ensure WallClockStartTime is initialized, so that it matches
    // LogStartTime.
    LogMessage::WallClockStartTime();
    log_line_.timestamp_ = webrtc::Timestamp::Millis(time);
  }

  if (LogMessage::log_thread_) {
    log_line_.thread_id_ = CurrentThreadId();
  }
}

void LogStreamer::Append(Rank3, const LogMetadata& value) {
  log_line_.severity_ = value.Severity();
  log_line_.filename_ = FilenameFromPath(value.File());
  log_line_.line_ = value.Line();
}

void LogStreamer::Append(Rank3, const LogMetadataErr& value) {
  Append(Rank3{}, value.meta);
  err_ctx_ = value.err_ctx;
  err_ = value.err;
}

void LogStreamer::FinishPrintStream() {
  if (err_ctx_ != ERRCTX_NONE) {
    char tmp_buf[1024];
    SimpleStringBuilder tmp(tmp_buf);
    tmp.AppendFormat(" : [0x%08X]", err_);

    switch (err_ctx_) {
      case ERRCTX_ERRNO:
        tmp << " " << strerror(err_);
        break;
#ifdef WEBRTC_WIN
      case ERRCTX_HRESULT: {
        char msgbuf[256];
        DWORD flags =
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        if (DWORD len = FormatMessageA(
                flags, nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                msgbuf, sizeof(msgbuf) / sizeof(msgbuf[0]), nullptr)) {
          while ((len > 0) &&
                 isspace(static_cast<unsigned char>(msgbuf[len - 1]))) {
            msgbuf[--len] = 0;
          }
          tmp << " " << msgbuf;
        }
        break;
      }
#endif  // WEBRTC_WIN
      default:
        break;
    }
    tmp << "\n";
    absl::StrAppend(&log_line_.message_, tmp_buf);
  } else {
    absl::StrAppend(&log_line_.message_, "\n");
  }
}

void LogStreamer::AppendMessage(absl::string_view value) {
  absl::StrAppend(&log_line_.message_, value);
}

void LogCall::Log(const LogLineRef& log_line) {
  if (log_line.severity() >= g_dbg_sev) {
    LogMessage::OutputToDebug(log_line);
  }

  webrtc::MutexLock lock(&GetLoggingLock());
  for (LogSink* entry = LogMessage::streams_; entry != nullptr;
       entry = entry->next_) {
    if (log_line.severity() >= entry->min_severity_) {
      entry->OnLogMessage(log_line);
    }
  }
}

}  // namespace webrtc_logging_impl

int LogMessage::GetMinLogSeverity() {
  return g_min_sev;
}

LoggingSeverity LogMessage::GetLogToDebug() {
  return g_dbg_sev;
}
int64_t LogMessage::LogStartTime() {
  static const int64_t g_start = SystemTimeMillis();
  return g_start;
}

uint32_t LogMessage::WallClockStartTime() {
  static const uint32_t g_start_wallclock = time(nullptr);
  return g_start_wallclock;
}

void LogMessage::LogThreads(bool on) {
  log_thread_ = on;
}

void LogMessage::LogTimestamps(bool on) {
  log_timestamp_ = on;
}

void LogMessage::LogToDebug(LoggingSeverity min_sev) {
  g_dbg_sev = min_sev;
  webrtc::MutexLock lock(&GetLoggingLock());
  UpdateMinLogSeverity();
}

void LogMessage::SetLogToStderr(bool log_to_stderr) {
  log_to_stderr_ = log_to_stderr;
}

int LogMessage::GetLogToStream(LogSink* stream) {
  webrtc::MutexLock lock(&GetLoggingLock());
  LoggingSeverity sev = LS_NONE;
  for (LogSink* entry = streams_; entry != nullptr; entry = entry->next_) {
    if (stream == nullptr || stream == entry) {
      sev = std::min(sev, entry->min_severity_);
    }
  }
  return sev;
}

void LogMessage::AddLogToStream(LogSink* stream, LoggingSeverity min_sev) {
  webrtc::MutexLock lock(&GetLoggingLock());
  stream->min_severity_ = min_sev;
  stream->next_ = streams_;
  streams_ = stream;
  streams_empty_.store(false, std::memory_order_relaxed);
  UpdateMinLogSeverity();
}

void LogMessage::RemoveLogToStream(LogSink* stream) {
  webrtc::MutexLock lock(&GetLoggingLock());
  for (LogSink** entry = &streams_; *entry != nullptr;
       entry = &(*entry)->next_) {
    if (*entry == stream) {
      *entry = (*entry)->next_;
      break;
    }
  }
  streams_empty_.store(streams_ == nullptr, std::memory_order_relaxed);
  UpdateMinLogSeverity();
}

void LogMessage::ConfigureLogging(absl::string_view params) {
  LoggingSeverity current_level = LS_VERBOSE;
  LoggingSeverity debug_level = GetLogToDebug();

  std::vector<std::string> tokens;
  tokenize(params, ' ', &tokens);

  for (const std::string& token : tokens) {
    if (token.empty())
      continue;

    // Logging features
    if (token == "tstamp") {
      LogTimestamps();
    } else if (token == "thread") {
      LogThreads();

      // Logging levels
    } else if (token == "verbose") {
      current_level = LS_VERBOSE;
    } else if (token == "info") {
      current_level = LS_INFO;
    } else if (token == "warning") {
      current_level = LS_WARNING;
    } else if (token == "error") {
      current_level = LS_ERROR;
    } else if (token == "none") {
      current_level = LS_NONE;

      // Logging targets
    } else if (token == "debug") {
      debug_level = current_level;
    }
  }

#if defined(WEBRTC_WIN) && !defined(WINUWP)
  if ((LS_NONE != debug_level) && !::IsDebuggerPresent()) {
    // First, attempt to attach to our parent's console... so if you invoke
    // from the command line, we'll see the output there.  Otherwise, create
    // our own console window.
    // Note: These methods fail if a console already exists, which is fine.
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
      ::AllocConsole();
  }
#endif  // defined(WEBRTC_WIN) && !defined(WINUWP)

  LogToDebug(debug_level);
}

void LogMessage::UpdateMinLogSeverity()
    RTC_EXCLUSIVE_LOCKS_REQUIRED(GetLoggingLock()) {
  LoggingSeverity min_sev = g_dbg_sev;
  for (LogSink* entry = streams_; entry != nullptr; entry = entry->next_) {
    min_sev = std::min(min_sev, entry->min_severity_);
  }
  g_min_sev = min_sev;
}

void LogMessage::OutputToDebug(const LogLineRef& log_line) {
  std::string msg_str = log_line.DefaultLogLine();
  bool log_to_stderr = log_to_stderr_;
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS) && defined(NDEBUG)
  // On the Mac, all stderr output goes to the Console log and causes clutter.
  // So in opt builds, don't log to stderr unless the user specifically sets
  // a preference to do so.
  CFStringRef domain = CFBundleGetIdentifier(CFBundleGetMainBundle());
  if (domain != nullptr) {
    Boolean exists_and_is_valid;
    Boolean should_log = CFPreferencesGetAppBooleanValue(
        CFSTR("logToStdErr"), domain, &exists_and_is_valid);
    // If the key doesn't exist or is invalid or is false, we will not log to
    // stderr.
    log_to_stderr = exists_and_is_valid && should_log;
  }
#endif  // defined(WEBRTC_MAC) && !defined(WEBRTC_IOS) && defined(NDEBUG)

#if defined(WEBRTC_WIN)
  // Always log to the debugger.
  // Perhaps stderr should be controlled by a preference, as on Mac?
  OutputDebugStringA(msg_str.c_str());
  if (log_to_stderr) {
    // This handles dynamically allocated consoles, too.
    if (HANDLE error_handle = ::GetStdHandle(STD_ERROR_HANDLE)) {
      log_to_stderr = false;
      DWORD written = 0;
      ::WriteFile(error_handle, msg_str.c_str(),
                  static_cast<DWORD>(msg_str.size()), &written, 0);
    }
  }
#endif  // WEBRTC_WIN

#if defined(WEBRTC_ANDROID)
  // Android's logging facility uses severity to log messages but we
  // need to map libjingle's severity levels to Android ones first.
  // Also write to stderr which maybe available to executable started
  // from the shell.
  int prio;
  switch (log_line.severity()) {
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

  int size = msg_str.size();
  int current_line = 0;
  int idx = 0;
  const int max_lines = size / kMaxLogLineSize + 1;
  if (max_lines == 1) {
    __android_log_print(prio, log_line.tag().data(), "%.*s", size,
                        msg_str.c_str());
  } else {
    while (size > 0) {
      const int len = std::min(size, kMaxLogLineSize);
      // Use the size of the string in the format (msg may have \0 in the
      // middle).
      __android_log_print(prio, log_line.tag().data(), "[%d/%d] %.*s",
                          current_line + 1, max_lines, len,
                          msg_str.c_str() + idx);
      idx += len;
      size -= len;
      ++current_line;
    }
  }
#endif  // WEBRTC_ANDROID
  if (log_to_stderr) {
    fprintf(stderr, "%s", msg_str.c_str());
    fflush(stderr);
  }
}

// static
bool LogMessage::IsNoop(LoggingSeverity severity) {
  if (severity >= g_dbg_sev || severity >= g_min_sev)
    return false;
  return streams_empty_.load(std::memory_order_relaxed);
}

}  // namespace rtc
#endif

namespace rtc {
// Default implementation, override is recomended.
void LogSink::OnLogMessage(const LogLineRef& log_line) {
#if defined(WEBRTC_ANDROID)
  OnLogMessage(log_line.DefaultLogLine(), log_line.severity(),
               log_line.tag().data());
#else
  OnLogMessage(log_line.DefaultLogLine(), log_line.severity());
#endif
}

// Inefficient default implementation, override is recommended.
void LogSink::OnLogMessage(const std::string& msg,
                           LoggingSeverity severity,
                           const char* tag) {
  OnLogMessage(absl::StrCat(tag, ": ", msg), severity);
}

void LogSink::OnLogMessage(const std::string& msg,
                           LoggingSeverity /* severity */) {
  OnLogMessage(msg);
}

// Inefficient default implementation, override is recommended.
void LogSink::OnLogMessage(absl::string_view msg,
                           LoggingSeverity severity,
                           const char* tag) {
  OnLogMessage(absl::StrCat(tag, ": ", msg), severity);
}

void LogSink::OnLogMessage(absl::string_view msg,
                           LoggingSeverity /* severity */) {
  OnLogMessage(msg);
}

void LogSink::OnLogMessage(absl::string_view msg) {
  OnLogMessage(std::string(msg));
}
}  // namespace rtc
