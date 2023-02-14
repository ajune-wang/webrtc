/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/location.h"

#include <stdio.h>

#if defined(COMPILER_MSVC)
#include <intrin.h>
#endif

#if defined(COMPILER_MSVC)
#define RETURN_ADDRESS() _ReturnAddress()
#elif defined(COMPILER_GCC) && !IS_NACL
#define RETURN_ADDRESS() \
  __builtin_extract_return_addr(__builtin_return_address(0))
#else
#define RETURN_ADDRESS() nullptr
#endif

namespace webrtc {

#ifdef RTC_ENABLE_LOCATION

Location Location::Current(const char* function_name,
                           const char* file_name,
                           int line_number) {
  return Location(function_name, file_name, line_number, RETURN_ADDRESS());
}

#endif  // RTC_ENABLE_LOCATION

}  // namespace webrtc
