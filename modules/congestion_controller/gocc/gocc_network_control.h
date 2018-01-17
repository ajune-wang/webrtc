/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOCC_GOCC_NETWORK_CONTROL_H_
#define MODULES_CONGESTION_CONTROLLER_GOCC_GOCC_NETWORK_CONTROL_H_

#include <deque>
#include <memory>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/bitrate_controller/send_side_bandwidth_estimation.h"
#include "modules/congestion_controller/gocc/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/gocc/alr_detector.h"
#include "modules/congestion_controller/gocc/delay_based_bwe.h"
#include "modules/congestion_controller/gocc/probe_controller.h"
#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "network_control/include/network_control.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/race_checker.h"

namespace webrtc {

class GoccNetworkController : public NetworkControllerInterface {
 public:
  GoccNetworkController(const Clock* clock, RtcEventLog* event_log);
  ~GoccNetworkController() override;
  TimeDelta GetProcessInterval() override;
  NetworkInformationReceivers GetReceivers() override;
  NetworkControlProducers GetProducers() override;

 private:
  void OnNetworkAvailability(NetworkAvailability);
  void OnNetworkRouteChange(NetworkRouteChange);
  void OnProcessInterval(ProcessInterval);
  void OnRemoteBitrateReport(RemoteBitrateReport);
  void OnRoundTripTimeReport(RoundTripTimeReport);
  void OnSentPacket(SentPacket);
  void OnStreamsConfig(StreamsConfig);
  void OnTargetRateConstraints(TargetRateConstraints);
  void OnTransportLossReport(TransportLossReport);
  void OnTransportPacketsFeedback(TransportPacketsFeedback);

 private:
  void MaybeUpdateCongestionWindow();

  void MaybeTriggerOnNetworkChanged();
  void OnNetworkEstimate(NetworkEstimate);
  void UpdatePacingRates();
  bool HasNetworkParametersToReportChanged(uint32_t bitrate_bps,
                                           uint8_t fraction_loss,
                                           int64_t rtt);
  bool GetNetworkParameters(int32_t* estimated_bitrate_bps,
                            uint8_t* fraction_loss,
                            int64_t* rtt_ms);

  CongestionWindow::SimpleJunction CongestionWindowJunction;
  PacerConfig::SimpleJunction PacerConfigJunction;
  ProbeClusterConfig::SimpleJunction ProbeClusterConfigJunction;
  TargetTransferRate::SimpleJunction TargetTransferRateJunction;

  const Clock* const clock_;
  RtcEventLog* const event_log_;

  const std::unique_ptr<ProbeController> probe_controller_;

  std::unique_ptr<SendSideBandwidthEstimation> bandwidth_estimation_;
  std::unique_ptr<AlrDetector> alr_detector_;
  std::unique_ptr<DelayBasedBwe> delay_based_bwe_;
  std::unique_ptr<AcknowledgedBitrateEstimator> acknowledged_bitrate_estimator_;

  std::deque<int64_t> feedback_rtts_;
  rtc::Optional<int64_t> min_feedback_rtt_ms_;

  rtc::Optional<NetworkEstimate> last_estimate_;
  rtc::Optional<TargetTransferRate> last_target_rate_;

  int32_t last_estimated_bitrate_bps_ = 0;
  uint8_t last_estimated_fraction_loss_ = 0;
  int64_t last_estimated_rtt_ms_ = 0;

  uint32_t last_reported_target_bitrate_bps_ = 0;
  uint8_t last_reported_fraction_loss_ = 0;
  int64_t last_reported_rtt_ms_ = 0;

  StreamsConfig streams_config_;

  bool in_cwnd_experiment_;
  int64_t accepted_queue_ms_;
  bool previously_in_alr = false;

  NetworkAvailability::MessageHandler NetworkAvailabilityHandler;
  NetworkRouteChange::MessageHandler NetworkRouteChangeHandler;
  ProcessInterval::MessageHandler ProcessIntervalHandler;
  RemoteBitrateReport::MessageHandler RemoteBitrateReportHandler;
  RoundTripTimeReport::MessageHandler RoundTripTimeReportHandler;
  SentPacket::MessageHandler SentPacketHandler;
  StreamsConfig::MessageHandler StreamsConfigHandler;
  TargetRateConstraints::MessageHandler TargetRateConstraintsHandler;
  TransportLossReport::MessageHandler TransportLossReportHandler;
  TransportPacketsFeedback::MessageHandler TransportPacketsFeedbackHandler;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(GoccNetworkController);
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOCC_GOCC_NETWORK_CONTROL_H_
