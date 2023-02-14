/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_LOCATION_H_
#define API_LOCATION_H_

#include <string>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

#ifdef RTC_ENABLE_LOCATION

// Location provides basic info where of an object was constructed, or was
// significantly brought to life.
// This is a stripped down version of:
// https://code.google.com/p/chromium/codesearch#chromium/src/base/location.h
class RTC_EXPORT Location {
 public:
  // Constructor should be called with a long-lived char*, such as __FILE__.
  // It assumes the provided value will persist as a global constant, and it
  // will not make a copy of it.
  Location(const char* function_name,
           const char* file_name,
           int line_number,
           const void* program_counter)
      : function_name_(function_name),
        file_name_(file_name),
        line_number_(line_number),
        program_counter_(program_counter) {}
  Location() = default;
  Location(const Location&) = default;
  Location(Location&&) = default;
  Location& operator=(Location&&) = default;

  const char* function_name() const { return function_name_; }
  const char* file_name() const { return file_name_; }
  int line_number() const { return line_number_; }
  const void* program_counter() const { return program_counter_; }

  static Location Current(const char* function_name = __builtin_FUNCTION(),
                          const char* file_name = __builtin_FILE(),
                          int line_number = __builtin_LINE());

 private:
  const char* function_name_ = "Unknown";
  const char* file_name_ = "Unknown";
  int line_number_ = -1;
  const void* program_counter_ = nullptr;
};

// Define a macro to record the current source location.
#define RTC_FROM_HERE \
  ::webrtc::Location::Current(__FUNCTION__, __FILE__, __LINE__)

#else  // RTC_ENABLE_LOCATION

class RTC_EXPORT Location {};
#define RTC_FROM_HERE ::webrtc::Location()

#endif  // RTC_ENABLE_LOCATION

}  // namespace webrtc

#endif  // API_LOCATION_H_
