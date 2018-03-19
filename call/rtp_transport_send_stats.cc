/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "call/rtp_transport_send_stats.h"
namespace webrtc {
namespace {
void MaybeUpdate(rtc::Optional<int64_t>* target,
                 const rtc::Optional<int64_t>& optional_value) {
  if (optional_value.has_value()) {
    *target = optional_value;
  }
}
}  // namespace

RtpTransportSendStats::RtpTransportSendStats() = default;
RtpTransportSendStats::RtpTransportSendStats(const RtpTransportSendStats&) =
    default;
RtpTransportSendStats::~RtpTransportSendStats() = default;

void RtpTransportSendStats::UpdateWith(const RtpTransportSendStats& other) {
  MaybeUpdate(&max_padding_bitrate_bps, other.max_padding_bitrate_bps);
  MaybeUpdate(&min_allocated_send_bitrate_bps,
              other.min_allocated_send_bitrate_bps);
  MaybeUpdate(&send_bandwidth_bps, other.send_bandwidth_bps);
  MaybeUpdate(&target_send_bitrate_bps, other.target_send_bitrate_bps);
  // MaybeUpdate(&pacer_queue_delay_ms, other.pacer_queue_delay_ms);
  MaybeUpdate(&round_trip_time_ms, other.round_trip_time_ms);
}

}  // namespace webrtc
