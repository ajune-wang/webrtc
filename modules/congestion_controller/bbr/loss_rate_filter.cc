/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/bbr/loss_rate_filter.h"

#include <cmath>

namespace webrtc {
namespace bbr {

LossRateFilter::LossRateFilter(TimeDelta filter_time)
    : filter_time_(filter_time.ToSecondsAsDouble()) {}

void LossRateFilter::UpdateWithLossStatus(Timestamp send_time,
                                          bool packet_lost) {
  double loss = packet_lost ? 1.0 : 0.0;
  double time_diff = (send_time - last_send_time_).ToSecondsAsDouble();
  last_send_time_ = send_time;
  RTC_CHECK_GE(time_diff, 0.0);

  double e = time_diff / filter_time_;
  loss_rate_estimate_ = -expm1(-e) * loss + exp(-e) * loss_rate_estimate_;
}

double LossRateFilter::GetLossRate() const {
  return loss_rate_estimate_;
}

}  // namespace bbr
}  // namespace webrtc
