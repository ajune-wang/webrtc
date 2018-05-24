/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_BBR_LOSS_RATE_FILTER_H_
#define MODULES_CONGESTION_CONTROLLER_BBR_LOSS_RATE_FILTER_H_

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {
namespace bbr {
class LossRateFilter {
 public:
  explicit LossRateFilter(TimeDelta filter_time);
  void UpdateWithLossStatus(Timestamp send_time, bool packet_lost);
  double GetLossRate() const;

 private:
  const double filter_time_;
  double loss_rate_estimate_ = 0.0;
  Timestamp last_send_time_ = Timestamp::seconds(0);
};

}  // namespace bbr
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_BBR_LOSS_RATE_FILTER_H_
