/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_DELAY_BASED_NETWORK_CONTROL_H_
#define MODULES_CONGESTION_CONTROLLER_DELAY_BASED_NETWORK_CONTROL_H_

#include <deque>
#include <memory>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/bitrate_controller/send_side_bandwidth_estimation.h"
#include "modules/congestion_controller/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/alr_detector.h"
#include "modules/congestion_controller/delay_based_bwe.h"
#include "modules/congestion_controller/network_controllers.h"
#include "modules/congestion_controller/probe_controller.h"
#include "modules/include/module.h"
#include "modules/include/module_common_types.h"
#include "network_control/network_controller.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/networkroute.h"
#include "rtc_base/race_checker.h"

namespace webrtc {
namespace network {

class DelayBasedNetworkController : public NetworkControllerInternalInterface {
 public:
  DelayBasedNetworkController(const Clock* clock,
                              RtcEventLog* event_log,
                              NetworkControlObservers::uptr observers);
  ~DelayBasedNetworkController() override;
  units::TimeDelta GetProcessInterval() override;
  void ConnectHandlers(NetworkControlHandlers::uptr) override;

 private:
  void OnSentPacket(SentPacket);
  void OnRemoteBitrateReport(RemoteBitrateReport);
  void OnRoundTripTimeReport(RoundTripTimeReport);
  void OnTransportLossReport(TransportLossReport);
  void OnStreamsConfig(StreamsConfig);

  void OnTransportPacketsFeedback(TransportPacketsFeedback);
  void OnNetworkRouteChange(NetworkRouteChange);

  void OnTransferRateConstraints(TargetRateConstraints);
  void OnNetworkAvailability(NetworkAvailability);

  void OnProcessInterval(ProcessInterval);

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
  const Clock* const clock_;
  RtcEventLog* const event_log_;

  NetworkControlObservers::uptr observers_;

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

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(DelayBasedNetworkController);
};

}  // namespace network
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_DELAY_BASED_NETWORK_CONTROL_H_
