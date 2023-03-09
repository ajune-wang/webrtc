/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/location.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

// static
Location Location::Current(const char* function_name,
                           const char* file_name,
                           int line_number) {
  return Location(function_name, file_name, line_number);
}

std::string Location::ToString() const {
  rtc::StringBuilder ss;
  ss << file_name << ": " << function_name << ":" << line_number;
  return ss.Release();
}

Location::Location(const char* function_name,
                   const char* file_name,
                   int line_number)
    : function_name(function_name),
      file_name(file_name),
      line_number(line_number) {}

}  // namespace webrtc
