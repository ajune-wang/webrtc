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

namespace webrtc {
namespace bbr {
namespace {
const int kLimitNumPackets = 20;
const int64_t kUpdateIntervalMs = 1000;
}  // namespace

LossRateFilter::LossRateFilter()
    : random_(55),
      lost_packets_since_last_loss_update_(0),
      expected_packets_since_last_loss_update_(0),
      loss_rate_estimate_(0.0),
      next_loss_update_ms_(0) {}

void LossRateFilter::UpdateWithLossStatus(int64_t feedback_time,
                                          int packets_sent,
                                          int packets_lost) {
  lost_packets_since_last_loss_update_ += packets_lost;
  expected_packets_since_last_loss_update_ += packets_sent;

  if (feedback_time >= next_loss_update_ms_ &&
      expected_packets_since_last_loss_update_ >= kLimitNumPackets) {
    int64_t lost = lost_packets_since_last_loss_update_;
    int64_t expected = expected_packets_since_last_loss_update_;
    loss_rate_estimate_ = static_cast<double>(lost) / expected;

    // Based on the RTCP Sender implementation.
    int64_t inteval_ms = kUpdateIntervalMs * (0.5 + random_.Rand<double>());

    next_loss_update_ms_ = feedback_time + inteval_ms;
    lost_packets_since_last_loss_update_ = 0;
    expected_packets_since_last_loss_update_ = 0;
  }
}

double LossRateFilter::GetLossRate() const {
  return loss_rate_estimate_;
}
}  // namespace bbr
}  // namespace webrtc
