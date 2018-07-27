/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_PCC_BITRATE_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_PCC_BITRATE_CONTROLLER_H_

#include <memory>
#include <vector>

#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "modules/congestion_controller/pcc/monitor_interval.h"
#include "modules/congestion_controller/pcc/utility_function.h"

namespace webrtc {
namespace pcc {

class PccBitrateController {
 public:
  PccBitrateController(double initial_conversion_factor,
                       double initial_dynamic_boundary,
                       double dynamic_boundary_increment,
                       double rtt_gradient_coefficient,
                       double loss_coefficient,
                       double throughput_coefficient,
                       double throughput_power,
                       double rtt_gradient_threshold);

  PccBitrateController(
      double initial_conversion_factor,
      double initial_dynamic_boundary,
      double dynamic_boundary_increment,
      std::unique_ptr<PccUtilityFunctionInterface> utility_function);

  DataRate ComputeRateUpdate(const std::vector<MonitorInterval>& block,
                             DataRate bandwidth_estimate);

  ~PccBitrateController();

 private:
  double ApplyDynamicBoundary(double rate_change, double bitrate);
  double ComputeStepSize(double utility_gradient);

  // Dynamic boundary variables:
  int64_t consecutive_rate_adjustments_number_;
  double initial_dynamic_boundary_;
  double dynamic_boundary_increment_;

  std::unique_ptr<PccUtilityFunctionInterface> utility_function_;
  // Step Size variables:
  int64_t step_size_adjustments_number_;
  double initial_conversion_factor_;

  // For slow start mode:
  absl::optional<double> previous_function_value;
};

}  // namespace pcc
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_PCC_BITRATE_CONTROLLER_H_
