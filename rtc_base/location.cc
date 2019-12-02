/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/location.h"

#include <stdio.h>

namespace rtc {

Location::Location(const char* function_name,
                   const char* file_and_line,
                   int line_number)
    : function_name_(function_name),
      file_name_(file_and_line),
      line_number_(line_number) {}

Location::Location() = default;

Location::Location(const Location&) = default;
Location& Location::operator=(const Location& other) = default;

std::string Location::ToString() const {
  char buf[256];
  snprintf(buf, sizeof(buf), "%s@%s:%d", function_name_, file_name_,
           line_number_);
  return buf;
}

}  // namespace rtc
