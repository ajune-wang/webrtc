/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "common_video/generic_frame_descriptor/generic_frame_info.h"

#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {

absl::InlinedVector<DecodeTargetIndication, 10>
GenericFrameInfo::DecodeTargetInfo(absl::string_view indication_symbols) {
  absl::InlinedVector<DecodeTargetIndication, 10> decode_targets;
  for (char symbol : indication_symbols) {
    DecodeTargetIndication indication;
    switch (symbol) {
      case '-':
        indication = DecodeTargetIndication::kNotPresent;
        break;
      case 'D':
        indication = DecodeTargetIndication::kDiscardable;
        break;
      case 'R':
        indication = DecodeTargetIndication::kRequired;
        break;
      case 'S':
        indication = DecodeTargetIndication::kSwitch;
        break;
      default:
        RTC_NOTREACHED();
    }
    decode_targets.push_back(indication);
  }

  return decode_targets;
}

GenericFrameInfo::GenericFrameInfo() = default;
GenericFrameInfo::GenericFrameInfo(const GenericFrameInfo&) = default;
GenericFrameInfo::~GenericFrameInfo() = default;

}  // namespace webrtc
