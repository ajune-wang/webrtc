/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_STATS_UTILS_H_
#define TEST_STATS_UTILS_H_

#include "api/stats/rtc_stats_report.h"

namespace webrtc {
namespace test {

template <typename T, typename U>
inline T GetStatOrDefault(const RTCStatsMember<T>& member, U default_value) {
  if (member.is_defined()) {
    return *member;
  }
  return default_value;
}

}  // namespace test
}  // namespace webrtc

#endif  // TEST_STATS_UTILS_H_
