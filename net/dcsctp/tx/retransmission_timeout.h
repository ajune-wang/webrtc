/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TX_RETRANSMISSION_TIMEOUT_H_
#define NET_DCSCTP_TX_RETRANSMISSION_TIMEOUT_H_

#include <cstdint>
#include <functional>

#include "net/dcsctp/public/dcsctp_options.h"

namespace dcsctp {

// Manages updating of the Retransmission Timeout (RTO) SCTP variable, which is
// used directly as the base timeout for T3-RTX and for other timers, such as
// delayed ack.
//
// When an round-trip-time (RTT) is calculated (outside this class), `Observe`
// is called, which calculates the retransmission timeout (RTO) value. The RTO
// value will become larger if the RTT is high and/or the RTT values are varying
// a lot, which is an indicator of a bad connection.
class RetransmissionTimeout {
 public:
  explicit RetransmissionTimeout(const DcSctpOptions& options);

  // To be called when a RTT has been measured, to update the RTO value.
  void ObserveRTT(int rtt_ms);

  // Returns the last measured RTT value.
  int last_rtt_ms() const { return last_rtt_ms_; }

  // Returns the Retransmission Timeout (RTO) value, in milliseconds.
  int rto_ms() const { return rto_ms_; }

  // Returns the smoothed RTT value, in milliseconds.
  int srtt_ms() const { return srtt_ms_; }

 private:
  const int min_rto_ms_;
  const int max_rto_ms_;
  // Last RTT
  int last_rtt_ms_ = 0;
  // Smoothed Round-Trip Time
  int srtt_ms_ = 0;
  // Round-Trip Time Variation
  int rttvar_ms_ = 0;
  // Retransmission Timeout
  int rto_ms_;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_TX_RETRANSMISSION_TIMEOUT_H_
