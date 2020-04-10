/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_NO_UNIQUE_ADDRESS_H_
#define RTC_BASE_SYSTEM_NO_UNIQUE_ADDRESS_H_

#include "rtc_base/checks.h"

// RTC_NO_UNIQUE_ADDRESS is a portable annotations to tell the compiler that
// a data member need not have an address distinct from all other non-static
// data members of its class.
// The macro expands to [[no_unique_address]] if the compiler supports the
// attribute, it expands to nothing otherwise.
// Clang supports this attribute since C++11, while other compilers should
// add support for it starting from C++20.
#if (defined(__clang__) || __cplusplus > 201703L) && !RTC_DCHECK_IS_ON
#define RTC_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define RTC_NO_UNIQUE_ADDRESS
#endif

#endif  // RTC_BASE_SYSTEM_NO_UNIQUE_ADDRESS_H_
