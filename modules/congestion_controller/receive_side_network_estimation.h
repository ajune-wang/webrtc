/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_RECEIVE_SIDE_NETWORK_ESTIMATION_H_
#define MODULES_CONGESTION_CONTROLLER_RECEIVE_SIDE_NETWORK_ESTIMATION_H_

#include <memory>
#include <string>

#include "api/task_queue/task_queue_factory.h"
#include "api/transport/field_trial_based_config.h"
#include "api/transport/network_control.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/source/rtcp_packet/network_estimate.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"
#include "rtc_base/task_queue.h"

namespace webrtc {
struct ReceiveSideNetworkEstimatorConfig {
  static constexpr char kFieldTrialKey[] = "WebRTC-ReceiveSideEstimation";
  FieldTrialFlag enabled{"Enabled"};
  FieldTrialParameter<TimeDelta> report_interval{"rep_int", TimeDelta::ms(500)};
  explicit ReceiveSideNetworkEstimatorConfig(
      WebRtcKeyValueConfig* trial_config) {
    ParseFieldTrial({&enabled, &report_interval},
                    trial_config->Lookup(kFieldTrialKey));
  }
};

class ReceiveSideNetworkEstimator {
 public:
  explicit ReceiveSideNetworkEstimator(
      TaskQueueFactory* task_queue_factory,
      PacketRouter* packet_router,
      NetworkStateEstimatorFactory* network_state_estimator_factory);

  void OnReceivedPacket(const ReceivedPacket& msg);

 private:
  FieldTrialBasedConfig trial_based_config_;
  const ReceiveSideNetworkEstimatorConfig conf_;
  PacketRouter* const packet_router_;
  const std::unique_ptr<NetworkStateEstimator> network_state_estimator_;
  Timestamp last_report_time = Timestamp::MinusInfinity();
  rtc::TaskQueue task_queue_;
  rtcp::NetworkEstimate prepared_estimate_;
};
}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_RECEIVE_SIDE_NETWORK_ESTIMATION_H_
