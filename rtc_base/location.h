/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_LOCATION_H_
#define RTC_BASE_LOCATION_H_

#include <string>

#include "rtc_base/stringize_macros.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {

// Location provides basic info where of an object was constructed, or was
// significantly brought to life.
// This is a stripped down version of:
// https://code.google.com/p/chromium/codesearch#chromium/src/base/location.h
class RTC_EXPORT Location {
 public:
  // Constructor should be called with a long-lived char*, such as __FILE__.
  // It assumes the provided value will persist as a global constant, and it
  // will not make a copy of it.
  Location(const char* function_name, const char* file_name);
  Location();
  Location(const Location& other);
  Location& operator=(const Location& other);

  const char* function_name() const { return function_name_; }
  const char* file_name() const { return file_name_; }
  // TODO(steveanton): Remove once all downstream users have been updated to use
  // |file_name()|.
  const char* file_and_line() const { return file_name_; }

  std::string ToString() const;

 private:
  const char* function_name_;
  const char* file_name_;
};

// Define a macro to record the current source location.
#define RTC_FROM_HERE RTC_FROM_HERE_WITH_FUNCTION(__FUNCTION__)

#define RTC_FROM_HERE_WITH_FUNCTION(function_name) \
  ::rtc::Location(function_name, __FILE__)

}  // namespace rtc

#endif  // RTC_BASE_LOCATION_H_
