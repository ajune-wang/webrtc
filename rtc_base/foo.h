/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FOO_H_
#define RTC_BASE_FOO_H_

#include "rtc_base/rtc_export.h"

int RTC_EXPORT(FOO) Foo();

// This will cause the following linker error with GN argument
// is_component_build=true:
//
// ld.lld: error: undefined symbol: Foo()
// >>> referenced by bar.cc:16 (../../rtc_base/bar.cc:16)
// >>>               obj/rtc_base/bar/bar.o:(main)
//
// int Foo();

#endif  // RTC_BASE_FOO_H_
