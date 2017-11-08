/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_CPU_INFO_H_
#define RTC_BASE_SYSTEM_CPU_INFO_H_

#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class CpuInfo {
 public:
  static uint32_t DetectNumberOfCores();

 private:
  CpuInfo() {}
};

}  // namespace webrtc

#endif  // RTC_BASE_SYSTEM_CPU_INFO_H_
