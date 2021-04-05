/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/retransmission_timeout.h"

#include <cmath>
#include <cstdint>

#include "net/dcsctp/public/dcsctp_options.h"

namespace dcsctp {
namespace {
// https://tools.ietf.org/html/rfc4960#section-15
constexpr float kRtoAlpha = 0.125;
constexpr float kRtoBeta = 0.25;
}  // namespace

RetransmissionTimeout::RetransmissionTimeout(const DcSctpOptions& options)
    : min_rto_ms_(options.rto_min_ms),
      max_rto_ms_(options.rto_max_ms),
      rto_ms_(options.rto_initial_ms) {}

void RetransmissionTimeout::ObserveRTT(int rtt_ms) {
  if (last_rtt_ms_ == 0) {
    // https://tools.ietf.org/html/rfc4960#section-6.3.1
    // "When the first RTT measurement R is made, set"
    srtt_ms_ = rtt_ms;
    rttvar_ms_ = rtt_ms / 2;
    rto_ms_ = srtt_ms_ + 4 * rttvar_ms_;
  } else {
    // https://tools.ietf.org/html/rfc4960#section-6.3.1
    // "When a new RTT measurement R' is made, set"
    int64_t diff_ms = std::abs(rtt_ms - srtt_ms_);
    rttvar_ms_ = (1 - kRtoBeta) * rttvar_ms_ + kRtoBeta * diff_ms;
    srtt_ms_ = (1 - kRtoAlpha) * srtt_ms_ + kRtoAlpha * rtt_ms;
    rto_ms_ = srtt_ms_ + 4 * rttvar_ms_;
  }
  if (rto_ms_ < min_rto_ms_) {
    rto_ms_ = min_rto_ms_;
  } else if (rto_ms_ > max_rto_ms_) {
    rto_ms_ = max_rto_ms_;
  }
  last_rtt_ms_ = rtt_ms;
}
}  // namespace dcsctp
