/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/pcc/utility_function.h"

#include <cmath>
#include <vector>

namespace webrtc {
namespace pcc {

VivaceUtilityFunction::VivaceUtilityFunction(double rtt_gradient_coefficient,
                                             double loss_coefficient,
                                             double throughput_coefficient,
                                             double throughput_power,
                                             double rtt_gradient_threshold)
    : rtt_gradient_coefficient_(rtt_gradient_coefficient),
      loss_coefficient_(loss_coefficient),
      throughput_power_(throughput_power),
      throughput_coefficient_(throughput_coefficient),
      rtt_gradient_threshold_(rtt_gradient_threshold) {}

double VivaceUtilityFunction::ComputeUtilityFunction(
    const MonitorInterval& monitor_interval) const {
  if (!monitor_interval.IsFeedbackCollectionDone()) {
    return 0;
  }
  double bitrate = monitor_interval.GetTargetSendingRate().kbps();
  double loss_rate = monitor_interval.GetLossRate();
  double rtt_gradient =
      monitor_interval.ComputeRttGradient(rtt_gradient_threshold_);
  return throughput_coefficient_ * std::pow(bitrate, throughput_power_) -
         rtt_gradient_coefficient_ * bitrate * rtt_gradient -
         loss_coefficient_ * bitrate * loss_rate;
}

VivaceUtilityFunction::~VivaceUtilityFunction() = default;

}  // namespace pcc
}  // namespace webrtc
