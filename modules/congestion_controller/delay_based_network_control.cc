/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/delay_based_network_control.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "modules/congestion_controller/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/alr_detector.h"
#include "modules/congestion_controller/probe_controller.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

using std::placeholders::_1;

using webrtc::network::units::Timestamp;
using webrtc::network::units::TimeDelta;
using webrtc::network::units::DataSize;
using webrtc::network::units::DataRate;

namespace webrtc {
namespace network {
namespace {

const char kCwndExperiment[] = "WebRTC-CwndExperiment";
const int64_t kDefaultAcceptedQueueMs = 250;

// Pacing-rate relative to our target send rate.
// Multiplicative factor that is applied to the target bitrate to calculate
// the number of bytes that can be transmitted per interval.
// Increasing this factor will result in lower delays in cases of bitrate
// overshoots from the encoder.
const float kDefaultPaceMultiplier = 2.5f;

bool CwndExperimentEnabled() {
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kCwndExperiment);
  // The experiment is enabled iff the field trial string begins with "Enabled".
  return experiment_string.find("Enabled") == 0;
}

bool ReadCwndExperimentParameter(int64_t* accepted_queue_ms) {
  RTC_DCHECK(accepted_queue_ms);
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kCwndExperiment);
  int parsed_values =
      sscanf(experiment_string.c_str(), "Enabled-%" PRId64, accepted_queue_ms);
  if (parsed_values == 1) {
    RTC_CHECK_GE(*accepted_queue_ms, 0)
        << "Accepted must be greater than or equal to 0.";
    return true;
  }
  return false;
}

// Makes sure that the bitrate and the min, max values are in valid range.
static void ClampBitrates(int64_t* bitrate_bps,
                          int64_t* min_bitrate_bps,
                          int64_t* max_bitrate_bps) {
  // TODO(holmer): We should make sure the default bitrates are set to 10 kbps,
  // and that we don't try to set the min bitrate to 0 from any applications.
  // The congestion controller should allow a min bitrate of 0.
  if (*min_bitrate_bps < congestion_controller::GetMinBitrateBps())
    *min_bitrate_bps = congestion_controller::GetMinBitrateBps();
  if (*max_bitrate_bps > 0)
    *max_bitrate_bps = std::max(*min_bitrate_bps, *max_bitrate_bps);
  if (*bitrate_bps > 0)
    *bitrate_bps = std::max(*min_bitrate_bps, *bitrate_bps);
}

std::vector<PacketFeedback> ReceivedPacketsFeedbackAsRtp(
    const network::TransportPacketsFeedback report) {
  std::vector<PacketFeedback> packet_feedback_vector;
  for (auto& fb : report.packet_feedbacks) {
    if (fb.receive_time.has_value()) {
      PacketFeedback pf(fb.receive_time->ms(), 0);
      pf.creation_time_ms = report.feedback_time.ms();
      if (fb.sent_packet.has_value()) {
        pf.payload_size = fb.sent_packet->size.bytes();
        pf.pacing_info = fb.sent_packet->pacing_info;
        pf.send_time_ms = fb.sent_packet->send_time.ms();
      } else {
        pf.send_time_ms = PacketFeedback::kNoSendTime;
      }
      packet_feedback_vector.push_back(pf);
    }
  }
  return packet_feedback_vector;
}

}  // namespace

std::unique_ptr<NetworkControllerInterface> CreateDelayBasedNetworkController(
    const Clock* clock,
    RtcEventLog* event_log,
    rtc::TaskQueue* task_queue) {
  NetworkControllerInternalInterface::uptr controller =
      rtc::MakeUnique<DelayBasedNetworkController>(clock, event_log);
  NetworkControlHandlingReceivers::uptr receivers =
      rtc::MakeUnique<TaskQueueNetworkControlReceivers>(task_queue,
                                                        controller.get());
  return rtc::MakeUnique<NetworkControllerWrapper>(std::move(controller),
                                                   std::move(receivers));
}

std::unique_ptr<NetworkControllerInterface> CreateDelayBasedNetworkController(
    const Clock* clock,
    RtcEventLog* event_log) {
  NetworkControllerInternalInterface::uptr controller =
      rtc::MakeUnique<DelayBasedNetworkController>(clock, event_log);
  NetworkControlHandlingReceivers::uptr receivers =
      rtc::MakeUnique<LockedNetworkControlReceivers>(controller.get());
  return rtc::MakeUnique<NetworkControllerWrapper>(std::move(controller),
                                                   std::move(receivers));
}

DelayBasedNetworkController::DelayBasedNetworkController(const Clock* clock,
                                                         RtcEventLog* event_log)
    : clock_(clock),
      event_log_(event_log),
      junctions_(rtc::MakeUnique<NetworkControlJunctions>()),
      probe_controller_(
          new ProbeController(clock_, &junctions_->ProbeClusterConfigJunction)),
      bandwidth_estimation_(
          rtc::MakeUnique<SendSideBandwidthEstimation>(event_log_)),
      alr_detector_(rtc::MakeUnique<AlrDetector>()),
      delay_based_bwe_(new DelayBasedBwe(event_log_, clock_)),
      acknowledged_bitrate_estimator_(
          rtc::MakeUnique<AcknowledgedBitrateEstimator>()),
      in_cwnd_experiment_(CwndExperimentEnabled()),
      accepted_queue_ms_(kDefaultAcceptedQueueMs) {
  streams_config_.pacing_factor = kDefaultPaceMultiplier;
  delay_based_bwe_->SetMinBitrate(congestion_controller::GetMinBitrateBps());
  if (in_cwnd_experiment_ &&
      !ReadCwndExperimentParameter(&accepted_queue_ms_)) {
    RTC_LOG(LS_WARNING) << "Failed to parse parameters for CwndExperiment "
                           "from field trial string. Experiment disabled.";
    in_cwnd_experiment_ = false;
  }
}

DelayBasedNetworkController::~DelayBasedNetworkController() {}

void DelayBasedNetworkController::ConnectHandlers(
    NetworkInformationHandlers::uptr receivers) {
  receivers->SentPacketHandler->SetHandler(
      std::bind(&DelayBasedNetworkController::OnSentPacket, this, _1));
  receivers->RemoteBitrateReportHandler->SetHandler(
      std::bind(&DelayBasedNetworkController::OnRemoteBitrateReport, this, _1));
  receivers->RoundTripTimeReportHandler->SetHandler(
      std::bind(&DelayBasedNetworkController::OnRoundTripTimeReport, this, _1));
  receivers->TransportLossReportHandler->SetHandler(
      std::bind(&DelayBasedNetworkController::OnTransportLossReport, this, _1));
  receivers->StreamsConfigHandler->SetHandler(
      std::bind(&DelayBasedNetworkController::OnStreamsConfig, this, _1));
  receivers->TransportPacketsFeedbackHandler->SetHandler(std::bind(
      &DelayBasedNetworkController::OnTransportPacketsFeedback, this, _1));
  receivers->NetworkRouteChangeHandler->SetHandler(
      std::bind(&DelayBasedNetworkController::OnNetworkRouteChange, this, _1));
  receivers->TransferRateConstraintsHandler->SetHandler(std::bind(
      &DelayBasedNetworkController::OnTransferRateConstraints, this, _1));
  receivers->NetworkAvailabilityHandler->SetHandler(
      std::bind(&DelayBasedNetworkController::OnNetworkAvailability, this, _1));
  receivers->ProcessIntervalHandler->SetHandler(
      std::bind(&DelayBasedNetworkController::OnProcessInterval, this, _1));
}

NetworkControlProducers::uptr DelayBasedNetworkController::GetProducers() {
  return junctions_->GetProducers();
}

void DelayBasedNetworkController::OnTransferRateConstraints(
    TargetRateConstraints constraints) {
  int64_t start_bitrate_bps = constraints.starting_rate.bps();
  int64_t min_bitrate_bps = constraints.min_data_rate.bps();
  int64_t max_bitrate_bps = constraints.max_data_rate.bps();

  ClampBitrates(&start_bitrate_bps, &min_bitrate_bps, &max_bitrate_bps);

  probe_controller_->SetBitrates(min_bitrate_bps, start_bitrate_bps,
                                 max_bitrate_bps);

  bandwidth_estimation_->SetBitrates(start_bitrate_bps, min_bitrate_bps,
                                     max_bitrate_bps);
  if (start_bitrate_bps > 0)
    delay_based_bwe_->SetStartBitrate(start_bitrate_bps);
  delay_based_bwe_->SetMinBitrate(min_bitrate_bps);

  MaybeTriggerOnNetworkChanged();
}

void DelayBasedNetworkController::OnNetworkRouteChange(NetworkRouteChange msg) {
  int64_t start_bitrate_bps = msg.constraints.starting_rate.bps();
  int64_t min_bitrate_bps = msg.constraints.min_data_rate.bps();
  int64_t max_bitrate_bps = msg.constraints.max_data_rate.bps();

  ClampBitrates(&start_bitrate_bps, &min_bitrate_bps, &max_bitrate_bps);

  bandwidth_estimation_ =
      rtc::MakeUnique<SendSideBandwidthEstimation>(event_log_);
  bandwidth_estimation_->SetBitrates(start_bitrate_bps, min_bitrate_bps,
                                     max_bitrate_bps);
  delay_based_bwe_.reset(new DelayBasedBwe(event_log_, clock_));
  acknowledged_bitrate_estimator_.reset(new AcknowledgedBitrateEstimator());
  delay_based_bwe_->SetStartBitrate(start_bitrate_bps);
  delay_based_bwe_->SetMinBitrate(min_bitrate_bps);

  probe_controller_->Reset();
  probe_controller_->SetBitrates(min_bitrate_bps, start_bitrate_bps,
                                 max_bitrate_bps);

  MaybeTriggerOnNetworkChanged();
}

void DelayBasedNetworkController::OnStreamsConfig(StreamsConfig msg) {
  bool pacing_changed =
      (msg.pacing_factor != streams_config_.pacing_factor) ||
      (msg.min_pacing_rate != streams_config_.min_pacing_rate) ||
      (msg.max_padding_rate != streams_config_.max_padding_rate);
  streams_config_ = msg;
  probe_controller_->EnablePeriodicAlrProbing(msg.requests_alr_probing);
  if (pacing_changed)
    UpdatePacingRates();
}

void DelayBasedNetworkController::OnNetworkAvailability(
    NetworkAvailability msg) {
  NetworkState state = msg.network_available ? kNetworkUp : kNetworkDown;
  probe_controller_->OnNetworkStateChanged(state);
}

void DelayBasedNetworkController::OnSentPacket(SentPacket sent_packet) {
  alr_detector_->OnBytesSent(sent_packet.size.bytes(),
                             sent_packet.send_time.ms());
}

units::TimeDelta DelayBasedNetworkController::GetProcessInterval() {
  const int64_t kUpdateIntervalMs = 25;
  return TimeDelta::ms(kUpdateIntervalMs);
}

void DelayBasedNetworkController::OnProcessInterval(ProcessInterval msg) {
  bandwidth_estimation_->UpdateEstimate(msg.at_time.ms());
  auto start_time_ms = alr_detector_->GetApplicationLimitedRegionStartTime();
  probe_controller_->SetAlrStartTimeMs(start_time_ms);
  probe_controller_->Process();
  MaybeTriggerOnNetworkChanged();
}

void DelayBasedNetworkController::OnTransportPacketsFeedback(
    TransportPacketsFeedback report) {
  int64_t feedback_rtt = -1;
  for (const auto& packet_feedback : report.packet_feedbacks) {
    if (packet_feedback.sent_packet.has_value() &&
        packet_feedback.receive_time.has_value()) {
      int64_t rtt = report.feedback_time.ms() -
                    packet_feedback.sent_packet->send_time.ms();
      // max() is used to account for feedback being delayed by the
      // receiver.
      feedback_rtt = std::max(rtt, feedback_rtt);
    }
  }
  if (feedback_rtt > -1) {
    feedback_rtts_.push_back(feedback_rtt);
    const size_t kFeedbackRttWindow = 32;
    if (feedback_rtts_.size() > kFeedbackRttWindow)
      feedback_rtts_.pop_front();
    min_feedback_rtt_ms_.emplace(
        *std::min_element(feedback_rtts_.begin(), feedback_rtts_.end()));
  }

  std::vector<PacketFeedback> received_feedback_vector =
      ReceivedPacketsFeedbackAsRtp(report);

  auto alr_start_time = alr_detector_->GetApplicationLimitedRegionStartTime();

  if (previously_in_alr && !alr_start_time.has_value()) {
    int64_t now_ms = clock_->TimeInMilliseconds();
    acknowledged_bitrate_estimator_->SetAlrEndedTimeMs(now_ms);
    probe_controller_->SetAlrEndedTimeMs(now_ms);
  }
  previously_in_alr = alr_start_time.has_value();
  acknowledged_bitrate_estimator_->IncomingPacketFeedbackVector(
      received_feedback_vector);
  DelayBasedBwe::Result result;
  result = delay_based_bwe_->IncomingPacketFeedbackVector(
      received_feedback_vector, acknowledged_bitrate_estimator_->bitrate_bps());
  if (result.updated) {
    if (result.probe) {
      bandwidth_estimation_->SetSendBitrate(result.target_bitrate_bps);
    }
    // Since SetSendBitrate now resets the delay-based estimate, we have to call
    // UpdateDelayBasedEstimate after SetSendBitrate.
    bandwidth_estimation_->UpdateDelayBasedEstimate(
        clock_->TimeInMilliseconds(), result.target_bitrate_bps);
    // Update the estimate in the ProbeController, in case we want to probe.
    MaybeTriggerOnNetworkChanged();
  }
  if (result.recovered_from_overuse) {
    probe_controller_->SetAlrStartTimeMs(alr_start_time);
    probe_controller_->RequestProbe();
  }
  MaybeUpdateCongestionWindow();
}

void DelayBasedNetworkController::MaybeUpdateCongestionWindow() {
  if (!in_cwnd_experiment_)
    return;
  // No valid RTT. Could be because send-side BWE isn't used, in which case
  // we don't try to limit the outstanding packets.
  if (!min_feedback_rtt_ms_)
    return;
  if (!last_estimate_.has_value())
    return;
  const DataSize kMinCwnd = DataSize::bytes(2 * 1500);
  TimeDelta time_window =
      TimeDelta::ms(*min_feedback_rtt_ms_ + accepted_queue_ms_);
  DataSize data_window = last_estimate_->bandwidth * time_window;
  CongestionWindow msg;
  msg.enabled = true;
  msg.data_window = std::max(kMinCwnd, data_window);
  junctions_->CongestionWindowJunction.OnMessage(msg);
  RTC_LOG(LS_INFO) << "Feedback rtt: " << *min_feedback_rtt_ms_
                   << " Bitrate: " << last_estimate_->bandwidth.bps();
}

void DelayBasedNetworkController::MaybeTriggerOnNetworkChanged() {
  int32_t estimated_bitrate_bps;
  uint8_t fraction_loss;
  int64_t rtt_ms;

  bool estimate_changed =
      GetNetworkParameters(&estimated_bitrate_bps, &fraction_loss, &rtt_ms);
  if (estimate_changed) {
    auto bwe_period = TimeDelta::ms(delay_based_bwe_->GetExpectedBwePeriodMs());

    NetworkEstimate new_estimate;
    new_estimate.at_time = Timestamp::us(clock_->TimeInMicroseconds());
    new_estimate.round_trip_time = TimeDelta::ms(rtt_ms);
    new_estimate.bandwidth = DataRate::bps(estimated_bitrate_bps);
    new_estimate.loss_rate_ratio = fraction_loss / 255.0f;
    new_estimate.bwe_period = bwe_period;
    new_estimate.changed = true;
    last_estimate_ = new_estimate;
    OnNetworkEstimate(new_estimate);
  }
}

void DelayBasedNetworkController::OnNetworkEstimate(NetworkEstimate estimate) {
  if (!estimate.changed)
    return;

  UpdatePacingRates();
  alr_detector_->SetEstimatedBitrate(estimate.bandwidth.bps());
  probe_controller_->SetEstimatedBitrate(estimate.bandwidth.bps());

  TargetTransferRate target_rate;
  // Lets use all the capacity we think we have!
  target_rate.target_rate = estimate.bandwidth;
  target_rate.basis_estimate = estimate;
  junctions_->TargetTransferRateJunction.OnMessage(target_rate);
}

bool DelayBasedNetworkController::GetNetworkParameters(
    int32_t* estimated_bitrate_bps,
    uint8_t* fraction_loss,
    int64_t* rtt_ms) {
  bandwidth_estimation_->CurrentEstimate(estimated_bitrate_bps, fraction_loss,
                                         rtt_ms);
  *estimated_bitrate_bps = std::max<int32_t>(
      *estimated_bitrate_bps, bandwidth_estimation_->GetMinBitrate());

  bool estimate_changed = false;
  if ((*estimated_bitrate_bps != last_estimated_bitrate_bps_) ||
      (*fraction_loss != last_estimated_fraction_loss_) ||
      (*rtt_ms != last_estimated_rtt_ms_)) {
    last_estimated_bitrate_bps_ = *estimated_bitrate_bps;
    last_estimated_fraction_loss_ = *fraction_loss;
    last_estimated_rtt_ms_ = *rtt_ms;
    estimate_changed = true;
  }

  BWE_TEST_LOGGING_PLOT(1, "fraction_loss_%", clock_->TimeInMilliseconds(),
                        (*fraction_loss * 100) / 256);
  BWE_TEST_LOGGING_PLOT(1, "rtt_ms", clock_->TimeInMilliseconds(), *rtt_ms);
  BWE_TEST_LOGGING_PLOT(1, "Target_bitrate_kbps", clock_->TimeInMilliseconds(),
                        *estimated_bitrate_bps / 1000);

  return estimate_changed;
}

void DelayBasedNetworkController::UpdatePacingRates() {
  if (!last_estimate_)
    return;
  DataRate pacing_rate =
      std::max(streams_config_.min_pacing_rate, last_estimate_->bandwidth) *
      streams_config_.pacing_factor;
  DataRate padding_rate =
      std::min(streams_config_.max_padding_rate, last_estimate_->bandwidth);
  PacerConfig msg;
  msg.at_time = Timestamp::us(clock_->TimeInMicroseconds());
  msg.time_window = TimeDelta::s(1);
  msg.data_window = pacing_rate * msg.time_window;
  msg.pad_window = padding_rate * msg.time_window;
  junctions_->PacerConfigJunction.OnMessage(msg);
}

void DelayBasedNetworkController::OnRemoteBitrateReport(
    RemoteBitrateReport msg) {
  bandwidth_estimation_->UpdateReceiverEstimate(msg.receive_time.ms(),
                                                msg.bandwidth.bps());
  BWE_TEST_LOGGING_PLOT(1, "REMB_kbps", msg.receive_time.ms(),
                        msg.bandwidth.bps() / 1000);
}

void DelayBasedNetworkController::OnRoundTripTimeReport(
    RoundTripTimeReport report) {
  bandwidth_estimation_->UpdateReceiverBlockRtt(report.round_trip_time.ms(),
                                                report.receive_time.ms());
  delay_based_bwe_->OnRttUpdate(report.round_trip_time.ms());
}

void DelayBasedNetworkController::OnTransportLossReport(
    TransportLossReport msg) {
  int64_t total_packets_delta =
      msg.packets_received_delta + msg.packets_lost_delta;
  bandwidth_estimation_->UpdateReceiverBlock(
      msg.packets_lost_delta, total_packets_delta, msg.receive_time.ms());
}
}  // namespace network
}  // namespace webrtc
