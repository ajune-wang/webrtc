/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/audio_format_conversion.h"

#include "absl/strings/match.h"
#include "rtc_base/checks.h"

namespace webrtc {

SdpAudioFormat CodecInstToSdp(const CodecInst& ci) {
  if (absl::EqualsIgnoreCase(ci.plname, "g722")) {
    RTC_CHECK_EQ(16000, ci.plfreq);
    RTC_CHECK(ci.channels == 1 || ci.channels == 2);
    return {"g722", 8000, ci.channels};
  } else if (absl::EqualsIgnoreCase(ci.plname, "opus")) {
    RTC_CHECK_EQ(48000, ci.plfreq);
    RTC_CHECK(ci.channels == 1 || ci.channels == 2);
    return ci.channels == 1
               ? SdpAudioFormat("opus", 48000, 2)
               : SdpAudioFormat("opus", 48000, 2, {{"stereo", "1"}});
  } else {
    return {ci.plname, ci.plfreq, ci.channels};
  }
}

}  // namespace webrtc
