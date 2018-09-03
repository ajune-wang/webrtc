/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/strings/slow_str_cat.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"

namespace rtc {
namespace slow_str_cat_impl {

namespace {

// There's one ArgScratch slot for each argument; find the extra scratch space
// that's located immediately after them.
char* FindBuffer(const ArgType* fmt, ArgScratch* scratch) {
  while (*fmt != ArgType::kEnd) {
    ++fmt;
    ++scratch;
  }
  return reinterpret_cast<char*>(scratch);
}

// Use absl::AlphaNum to convert `x` to a string. It would be much more
// convenient to use the building blocks that AlphaNum uses internally, but
// they're internal to Abseil.
absl::string_view ToString(const absl::AlphaNum& x, char** buf) {
  char* const location = *buf;
  (*buf) += x.size();
  std::memcpy(location, x.data(), x.size());
  return {location, x.size()};
}

}  // namespace

std::string SlowStrCatImpl(const ArgType* const fmt,
                           ArgScratch* const scratch,
                           ...) {
  size_t total_length = 0;
  char* buf = FindBuffer(fmt, scratch);

  const auto handle_arg = [&](size_t i, const absl::string_view sv) {
    scratch[i] = {sv.data(), sv.size()};
    total_length += sv.size();
  };

  // We need to use these generally forbidden types below. Make aliases for
  // them here, so that the NOLINT annotations can be in one place and not mess
  // up the formatting in the real code.
  using Long = long;                     // NOLINT(runtime/int)
  using ULong = unsigned long;           // NOLINT(runtime/int)
  using LongLong = long long;            // NOLINT(runtime/int)
  using ULongLong = unsigned long long;  // NOLINT(runtime/int)

  va_list args;
  va_start(args, scratch);
  for (size_t i = 0; fmt[i] != ArgType::kEnd; ++i) {
    switch (fmt[i]) {
      case ArgType::kCharP:
        handle_arg(i, va_arg(args, const char*));
        break;
      case ArgType::kStringView:
        handle_arg(i, *va_arg(args, const absl::string_view*));
        break;
      case ArgType::kStdString:
        handle_arg(i, *va_arg(args, const std::string*));
        break;
      case ArgType::kInt:
        handle_arg(i, ToString(va_arg(args, int), &buf));
        break;
      case ArgType::kLong:
        handle_arg(i, ToString(va_arg(args, Long), &buf));
        break;
      case ArgType::kLongLong:
        handle_arg(i, ToString(va_arg(args, LongLong), &buf));
        break;
      case ArgType::kUInt:
        handle_arg(i, ToString(va_arg(args, unsigned), &buf));
        break;
      case ArgType::kULong:
        handle_arg(i, ToString(va_arg(args, ULong), &buf));
        break;
      case ArgType::kULongLong:
        handle_arg(i, ToString(va_arg(args, ULongLong), &buf));
        break;
      case ArgType::kDouble:
        handle_arg(i, ToString(va_arg(args, double), &buf));
        break;
      case ArgType::kHex:
        handle_arg(i, ToString(*va_arg(args, const absl::Hex*), &buf));
        break;
      case ArgType::kDec:
        handle_arg(i, ToString(*va_arg(args, const absl::Dec*), &buf));
        break;
      default:
        RTC_NOTREACHED();
        va_end(args);
        return {};
    }
  }
  va_end(args);

  std::string result(total_length, '\0');
  char* str = &result[0];
  for (size_t i = 0; fmt[i] != ArgType::kEnd; ++i) {
    const size_t size = scratch[i].size;
    std::memcpy(str, scratch[i].data, size);
    str += size;
  }
  RTC_DCHECK_EQ(result.size(), str - &result[0]);

  return result;
}

}  // namespace slow_str_cat_impl
}  // namespace rtc
