/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_capture_metrics_helper.h"

#include "modules/desktop_capture/desktop_capture_types.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

void RecordCapturerImpl(uint32_t capturer_id) {
  RTC_HISTOGRAM_ENUMERATION_SPARSE("WebRTC.DesktopCapture.DesktopCapturerImpl",
                                   capturer_id, DesktopCapturerId::kMaxValue);
}

}  // namespace webrtc
