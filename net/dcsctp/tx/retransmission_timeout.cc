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
    : min_rto_(options.rto_min),
      max_rto_(options.rto_max),
      rto_(options.rto_initial) {}

void RetransmissionTimeout::ObserveRTT(DurationMs rtt) {
  if (last_rtt_ == DurationMs(0)) {
    // https://tools.ietf.org/html/rfc4960#section-6.3.1
    // "When the first RTT measurement R is made, set"
    srtt_ = rtt;
    rttvar_ = DurationMs(*rtt / 2);
    rto_ = DurationMs(*srtt_ + 4 * *rttvar_);
  } else {
    // https://tools.ietf.org/html/rfc4960#section-6.3.1
    // "When a new RTT measurement R' is made, set"
    int64_t diff = std::abs(*rtt - *srtt_);
    rttvar_ = DurationMs((1 - kRtoBeta) * *rttvar_ + kRtoBeta * diff);
    srtt_ = DurationMs((1 - kRtoAlpha) * *srtt_ + kRtoAlpha * *rtt);
    rto_ = DurationMs(*srtt_ + 4 * *rttvar_);
  }
  if (rto_ < min_rto_) {
    rto_ = min_rto_;
  } else if (rto_ > max_rto_) {
    rto_ = max_rto_;
  }
  last_rtt_ = rtt;
}
}  // namespace dcsctp
