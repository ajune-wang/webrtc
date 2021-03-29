/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SYSTEM_WRAPPERS_INCLUDE_NTP_TIME_UTIL_H_
#define SYSTEM_WRAPPERS_INCLUDE_NTP_TIME_UTIL_H_

#include "modules/rtp_rtcp/source/time_util.h"

// This is currently only a redirection to the functions defined in
// modules/rtp_rtcp/source/time_util.h, The next step is to move
// modules/rtp_rtcp/source/time_util.h|cc to system_wrapper.
// This, as an intermediate step is only for upstream projects that
// depend on modules/rtp_rtcp/source/time_util.h to switch to depend on
// system_wrapper instead.

#endif  // SYSTEM_WRAPPERS_INCLUDE_NTP_TIME_UTIL_H_
