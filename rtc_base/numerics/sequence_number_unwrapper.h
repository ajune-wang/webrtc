/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_SEQUENCE_NUMBER_UNWRAPPER_H_
#define RTC_BASE_NUMERICS_SEQUENCE_NUMBER_UNWRAPPER_H_

#include "rtc_base/numerics/sequence_number_util.h"

namespace webrtc {

using RtpTimestampUnwrapper = SeqNumUnwrapper<uint32_t>;

}

#endif  // RTC_BASE_NUMERICS_SEQUENCE_NUMBER_UNWRAPPER_H_
