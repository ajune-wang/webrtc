/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTCP_STATISTICS_H_
#define MODULES_RTP_RTCP_INCLUDE_RTCP_STATISTICS_H_

#include <stdint.h>

#include "absl/strings/string_view.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {

// Statistics for RTCP packet types.
struct RtcpPacketTypeCounter {
  RtcpPacketTypeCounter()
      : nack_packets(0),
        fir_packets(0),
        pli_packets(0),
        nack_requests(0),
        unique_nack_requests(0) {}

  void Add(const RtcpPacketTypeCounter& other) {
    nack_packets += other.nack_packets;
    fir_packets += other.fir_packets;
    pli_packets += other.pli_packets;
    nack_requests += other.nack_requests;
    unique_nack_requests += other.unique_nack_requests;
  }

  void Subtract(const RtcpPacketTypeCounter& other) {
    nack_packets -= other.nack_packets;
    fir_packets -= other.fir_packets;
    pli_packets -= other.pli_packets;
    nack_requests -= other.nack_requests;
    unique_nack_requests -= other.unique_nack_requests;
  }

  int UniqueNackRequestsInPercent() const {
    if (nack_requests == 0) {
      return 0;
    }
    return static_cast<int>((unique_nack_requests * 100.0f / nack_requests) +
                            0.5f);
  }

  uint32_t nack_packets;          // Number of RTCP NACK packets.
  uint32_t fir_packets;           // Number of RTCP FIR packets.
  uint32_t pli_packets;           // Number of RTCP PLI packets.
  uint32_t nack_requests;         // Number of NACKed RTP packets.
  uint32_t unique_nack_requests;  // Number of unique NACKed RTP packets.
};

class RtcpPacketTypeCounterObserver {
 public:
  virtual ~RtcpPacketTypeCounterObserver() {}
  virtual void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) = 0;
};

// Invoked for each cname passed in RTCP SDES blocks.
class RtcpCnameCallback {
 public:
  virtual ~RtcpCnameCallback() = default;

  virtual void OnCname(uint32_t ssrc, absl::string_view cname) = 0;
};

// Stats for RTCP sender reports (SR) for a specific SSRC.
// Refer to https://tools.ietf.org/html/rfc3550#section-6.4.1.
struct RtcpSenderReportStats {
  // Arrival NTP timestamp for the last received RTCP SR.
  NtpTime last_arrival_timestamp;
  // Received (a.k.a., remote) NTP timestamp for the last received RTCP SR.
  NtpTime last_remote_timestamp;
  // Total number of RTP data packets transmitted by the sender since starting
  // transmission up until the time this SR packet was generated. The count
  // should be reset if the sender changes its SSRC identifier.
  uint32_t packets_sent;
  // Total number of payload octets (i.e., not including header or padding)
  // transmitted in RTP data packets by the sender since starting transmission
  // up until the time this SR packet was generated. The count should be reset
  // if the sender changes its SSRC identifier.
  uint64_t bytes_sent;
  // Total number of RTCP SR blocks received.
  // https://www.w3.org/TR/webrtc-stats/#dom-rtcremoteoutboundrtpstreamstats-reportssent.
  uint64_t reports_count;
};

class RtcpSenderReportStatsCallback {
 public:
  virtual ~RtcpSenderReportStatsCallback() = default;

  virtual void OnSenderReport(const RtcpSenderReportStats& rtcp_sr_stats) = 0;
};

}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_INCLUDE_RTCP_STATISTICS_H_
