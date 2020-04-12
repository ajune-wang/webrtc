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

// RTC_NO_UNIQUE_ADDRESS is a portable annotation to tell the compiler that
// a data member need not have an address distinct from all other non-static
// data members of its class.
// It allows empty types to actually occupy zero bytes as class members,
// instead of occupying at least one byte just so that they get their own
// address. There is almost never any reason not to use it on class members
// that could possibly be empty.
// The macro expands to [[no_unique_address]] if the compiler supports the
// attribute, it expands to nothing otherwise.
// Clang supports this attribute since C++11, while other compilers should
// add support for it starting from C++20.
// This attribute is not supported by clang-cl yet, so the macro expands
// to nothing on Windows platforms.
#if (defined(__clang__) && !defined(_MSC_VER)) || __cplusplus > 201703L
#define RTC_NO_UNIQUE_ADDRESS \
  [[no_unique_address]]  // NOLINT(whitespace/braces)
#else
#define RTC_NO_UNIQUE_ADDRESS
#endif

#endif  // RTC_BASE_SYSTEM_NO_UNIQUE_ADDRESS_H_
