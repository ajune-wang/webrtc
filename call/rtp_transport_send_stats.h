/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_RTP_TRANSPORT_SEND_STATS_H_
#define CALL_RTP_TRANSPORT_SEND_STATS_H_
#include "api/optional.h"

namespace webrtc {
struct RtpTransportSendStats {
  RtpTransportSendStats();
  RtpTransportSendStats(const RtpTransportSendStats&);
  ~RtpTransportSendStats();
  rtc::Optional<int64_t> max_padding_bitrate_bps;
  rtc::Optional<int64_t> min_allocated_send_bitrate_bps;
  rtc::Optional<int64_t> send_bandwidth_bps;
  rtc::Optional<int64_t> target_send_bitrate_bps;
  // TODO(srte): Add pacer queue delay to this struct.
  // rtc::Optional<int64_t> pacer_queue_delay_ms;
  rtc::Optional<int64_t> round_trip_time_ms;
  void UpdateWith(const RtpTransportSendStats& other);
};
}  // namespace webrtc

#endif  // CALL_RTP_TRANSPORT_SEND_STATS_H_
