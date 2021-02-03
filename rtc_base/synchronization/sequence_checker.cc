/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/synchronization/sequence_checker.h"

#include "rtc_base/synchronization/sequence_checker_internal.h"

namespace webrtc {

std::string ExpectationToString(const webrtc::SequenceChecker* checker) {
#if RTC_DCHECK_IS_ON
  return checker->ExpectationToString();
#endif
  return std::string();
}

}  // namespace webrtc
