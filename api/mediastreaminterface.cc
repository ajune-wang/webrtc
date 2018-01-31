/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/mediastreaminterface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

const char MediaStreamTrackInterface::kVideoKind[] = "video";
const char MediaStreamTrackInterface::kAudioKind[] = "audio";

// TODO(ivoc): Remove this when the function becomes pure virtual.
AudioProcessorInterface::AudioProcessorStatistics
AudioProcessorInterface::GetStats(bool /*has_remote_tracks*/) {
  RTC_NOTREACHED() << "GetStats() is called but it has no "
                   << "implementation.";
  RTC_LOG(LS_ERROR) << "GetStats() is called but it has no "
                    << "implementation.";
  return AudioProcessorStatistics();
}

}  // namespace webrtc
