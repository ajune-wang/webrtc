/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/receive_side_network_estimation.h"

namespace webrtc {

constexpr char ReceiveSideNetworkEstimatorConfig::kFieldTrialKey[];

ReceiveSideNetworkEstimator::ReceiveSideNetworkEstimator(
    TaskQueueFactory* task_queue_factory,
    PacketRouter* packet_router,
    NetworkStateEstimatorFactory* network_state_estimator_factory)
    : conf_(&trial_based_config_),
      packet_router_(packet_router),
      network_state_estimator_(
          conf_.enabled && network_state_estimator_factory
              ? network_state_estimator_factory->Create(&trial_based_config_)
              : nullptr),
      task_queue_(task_queue_factory->CreateTaskQueue(
          "RecvSideEstimation",
          TaskQueueFactory::Priority::NORMAL)) {}

void ReceiveSideNetworkEstimator::OnReceivedPacket(const ReceivedPacket& msg) {
  if (!network_state_estimator_)
    return;
  task_queue_.PostTask([this, msg] {
    auto new_estimate = network_state_estimator_->ProcessReceivedPacket(msg);
    if (new_estimate &&
        msg.receive_time - last_report_time > conf_.report_interval) {
      last_report_time = msg.receive_time;
      prepared_estimate_.SetEstimate(*new_estimate);
      packet_router_->SendNetworkStateEstimate(prepared_estimate_);
    }
  });
}

}  // namespace webrtc
