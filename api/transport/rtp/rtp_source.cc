/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/rtp/rtp_source.h"

namespace webrtc {

RtpSource::RtpSource(int64_t timestamp_ms,
                     uint32_t source_id,
                     RtpSourceType source_type,
                     absl::optional<uint8_t> audio_level,
                     uint32_t rtp_timestamp)
    : timestamp_ms_(timestamp_ms),
      source_id_(source_id),
      source_type_(source_type),
      audio_level_(audio_level),
      rtp_timestamp_(rtp_timestamp) {}

}  // namespace webrtc
