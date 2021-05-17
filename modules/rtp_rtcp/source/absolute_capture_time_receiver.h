/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_ABSOLUTE_CAPTURE_TIME_RECEIVER_H_
#define MODULES_RTP_RTCP_SOURCE_ABSOLUTE_CAPTURE_TIME_RECEIVER_H_

#include "modules/rtp_rtcp/source/absolute_capture_time_interpolator.h"

namespace webrtc {

// DEPRECATED
class AbsoluteCaptureTimeReceiver: public AbsoluteCaptureTimeInterpolator {
 public:
  void SetRemoteToLocalClockOffset(absl::optional<int64_t> value_q32x32) {}
}

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_ABSOLUTE_CAPTURE_TIME_RECEIVER_H_
