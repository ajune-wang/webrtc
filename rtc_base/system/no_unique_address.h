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

#if (defined(__clang__) || __cplusplus > 201703L) && !RTC_DCHECK_IS_ON
#define RTC_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define RTC_NO_UNIQUE_ADDRESS
#endif

#endif  // RTC_BASE_SYSTEM_NO_UNIQUE_ADDRESS_H_
